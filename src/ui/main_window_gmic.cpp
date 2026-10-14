// MainWindow's G'MIC Filter Library (docs/gmic.md): the command that opens the
// library dialog with a live preview on the active layer, and running a chosen
// filter through the engine, limited to the selection and undoable.
//
// Every engine run happens off the UI thread, pixel conversion included. The
// dialog's preview follows the shared pattern of the filter dialogs
// (AsyncPixelPreviewState, the canvas "Rendering preview..." badge), and also
// asks a run that newer settings have made stale to stop.
//
// Layers: a filter changes every layer its input mode names that may be changed
// (editable pixels, not locked); text, shape and Smart Object layers are only read.
// A filter made for one image runs on each chosen layer alone, at its own size; a
// filter made for several layers gets them lined up on the document. Outputs beyond
// the ones that replace layers become new layers.

#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include "core/environment.hpp"
#include "core/layer_metadata.hpp"
#include "core/layer_tree.hpp"
#include "core/pixel_depth.hpp"
#include "core/vector_shape.hpp"
#include "filters/gmic/gmic_catalog.hpp"
#include "filters/gmic/gmic_engine.hpp"
#include "ui/background_workers.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gmic_filter_library_dialog.hpp"
#include "ui/gmic_library.hpp"
#include "ui/memory_info.hpp"
#include "ui/qt_geometry.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QPointF>
#include <QProgressDialog>
#include <QRectF>
#include <QRegion>
#include <QScopeGuard>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cctype>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <utility>
#include <vector>

namespace patchy::ui {

// A layer a filter may read, as it was before any preview touched it.
struct GmicLayerSnapshot {
  LayerId id{};
  std::shared_ptr<const PixelBuffer> pixels;
  Rect bounds{};
  std::string name;
  bool visible{true};  // shown in the document: its own eye and every enclosing group's
  // The filter may change it: editable RGB pixels, not locked. Text, shape and Smart
  // Object layers and locked ones are only read.
  bool writable{false};
};

// What one run needs, all immutable or copied so a worker can hold it.
struct GmicRunInput {
  // False: the filter runs once on each writable layer of `layers`, alone and at its own
  // size, as if applied to each in turn. True (a filter made for several layers reading
  // several): it runs once on all of `layers`, each placed on a document-sized image so
  // they line up.
  bool document_space{false};
  // Below 1: a draft. The filter runs on images reduced by this factor and each changed
  // layer's result is enlarged back to its full frame; layers a filter adds are left out.
  // Sizes a filter takes in pixels (a blur radius) are not rescaled, so a draft only
  // approximates the full run.
  double scale{1.0};
  std::vector<GmicLayerSnapshot> layers;  // top of the stack first
  // Where the active layer sits in `layers`; none when the input mode leaves it out
  // (a visible active layer with "all hidden layers").
  std::optional<std::size_t> active_input;
  LayerId active{};  // results with no layer of their own go below it
  std::int32_t document_width{0};
  std::int32_t document_height{0};
  FilterSelection selection;
  std::string command;
  std::string filter_name;      // names an output layer the filter left unnamed
  PixelFormat new_layer_format; // RGBA at the document's bit depth
};

// A layer's new pixels.
struct GmicLayerResult {
  LayerId id{};
  PixelBuffer pixels;
  Rect bounds{};
  Rect original_bounds{};
};

// A further output of a filter, to be inserted as a new layer.
struct GmicNewLayer {
  PixelBuffer pixels;
  Rect bounds{};
  std::string name;
  BlendMode blend_mode{BlendMode::Normal};
  float opacity{1.0F};
  // G'MIC lists images top of the stack first, so a further output goes below the layer
  // it belongs to, several for one layer each below the last (insert_gmic_new_layers).
  LayerId anchor{};
  // A previewed copy's eye (the Layers panel stays clickable for eyes during a preview);
  // the layer OK adds keeps it.
  bool visible{true};
};

// One filter run turned back into layer pixels. Built on a worker thread.
struct GmicRenderResult {
  bool ok{false};
  bool aborted{false};
  // A filter resized its image while a selection exists; nothing was produced.
  bool size_refused{false};
  // The input mode named no layer; nothing ran.
  bool no_input{false};
  // Every layer the input mode named is one the filter may not change; nothing ran.
  bool nothing_writable{false};
  // The run would need more memory than the computer can spare; nothing ran.
  std::int64_t memory_refused_mb{0};
  std::string error;   // the interpreter's message when it failed
  std::string status;  // what the filter returned for its controls
  std::vector<GmicLayerResult> layers;   // the layers whose pixels changed, top first
  std::vector<GmicNewLayer> new_layers;  // in insertion order
  // Chosen layers left as they were because they may not be changed.
  std::size_t read_only_layers{0};
  // A filter made for several layers combined them into the active layer.
  bool merged{false};

  [[nodiscard]] bool changed() const { return !layers.empty() || !new_layers.empty(); }
};

namespace {

// A layer's pixels as the planar float image G'MIC works on: display-encoded
// values on the 0..255 scale at every bit depth, alpha kept when the layer has one.
[[nodiscard]] gmic::Image gmic_image_from_pixels(const PixelBuffer& pixels) {
  const int width = pixels.width();
  const int height = pixels.height();
  const int channels = pixels.format().channels >= 4 ? 4 : 3;
  gmic::Image image = gmic::make_image(width, height, channels);
  std::vector<float> row(static_cast<std::size_t>(width) * 4U);
  for (int y = 0; y < height; ++y) {
    load_rgba_row(pixels, y, 0, width, DeepDomain::Encoded, row);
    for (int x = 0; x < width; ++x) {
      for (int channel = 0; channel < channels; ++channel) {
        image.at(x, y, channel) = row[static_cast<std::size_t>(x) * 4U + static_cast<std::size_t>(channel)];
      }
    }
  }
  return image;
}

// A layer placed on a transparent document-sized RGBA image, clipped to the document.
[[nodiscard]] gmic::Image gmic_document_image(const PixelBuffer& pixels, Rect bounds, int width, int height) {
  gmic::Image image = gmic::make_image(width, height, 4);
  const int left = std::max(0, bounds.x);
  const int top = std::max(0, bounds.y);
  const int right = std::min(width, bounds.x + pixels.width());
  const int bottom = std::min(height, bounds.y + pixels.height());
  if (right <= left || bottom <= top) {
    return image;
  }
  std::vector<float> row(static_cast<std::size_t>(right - left) * 4U);
  for (int y = top; y < bottom; ++y) {
    load_rgba_row(pixels, y - bounds.y, left - bounds.x, right - left, DeepDomain::Encoded, row);
    for (int x = left; x < right; ++x) {
      for (int channel = 0; channel < 4; ++channel) {
        image.at(x, y, channel) = row[static_cast<std::size_t>(x - left) * 4U + static_cast<std::size_t>(channel)];
      }
    }
  }
  return image;
}

// A G'MIC result in a layer format. G'MIC returns 1 (gray), 2 (gray and alpha),
// 3 (RGB) or 4 (RGBA) channels; a result without alpha is opaque.
[[nodiscard]] PixelBuffer pixels_from_gmic_image(const gmic::Image& image, PixelFormat format) {
  PixelBuffer pixels(image.width, image.height, format);
  std::vector<float> row(static_cast<std::size_t>(image.width) * 4U);
  const bool gray = image.channels <= 2;
  const int alpha_channel = image.channels == 2 ? 1 : image.channels >= 4 ? 3 : -1;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto at = static_cast<std::size_t>(x) * 4U;
      for (int channel = 0; channel < 3; ++channel) {
        row[at + static_cast<std::size_t>(channel)] = image.at(x, y, gray ? 0 : channel);
      }
      row[at + 3U] = alpha_channel >= 0 ? image.at(x, y, alpha_channel) : 255.0F;
    }
    store_rgba_row(pixels, y, 0, image.width, DeepDomain::Encoded, row);
  }
  return pixels;
}

// `source` at `bounds` on a transparent document-sized buffer of the same format.
[[nodiscard]] PixelBuffer embed_in_document(const PixelBuffer& source, Rect bounds, int width, int height) {
  PixelBuffer canvas(width, height, source.format());
  const int left = std::max(0, bounds.x);
  const int top = std::max(0, bounds.y);
  const int right = std::min(width, bounds.x + source.width());
  const int bottom = std::min(height, bounds.y + source.height());
  if (right <= left || bottom <= top) {
    return canvas;
  }
  const auto pixel_bytes = static_cast<std::size_t>(bytes_per_pixel(source.format()));
  for (int y = top; y < bottom; ++y) {
    std::memcpy(canvas.pixel(left, y), source.pixel(left - bounds.x, y - bounds.y),
                static_cast<std::size_t>(right - left) * pixel_bytes);
  }
  return canvas;
}

// Crops a document-sized result to the pixels that are not fully transparent, so a
// layer does not grow to the whole document. Formats without alpha are left alone,
// as is a result with nothing in it.
void trim_to_content(PixelBuffer& pixels, Rect& bounds) {
  if (pixels.format().channels < 4 || pixels.width() <= 0 || pixels.height() <= 0) {
    return;
  }
  const int width = pixels.width();
  const int height = pixels.height();
  std::vector<float> row(static_cast<std::size_t>(width) * 4U);
  int left = width;
  int right = -1;
  int top = height;
  int bottom = -1;
  for (int y = 0; y < height; ++y) {
    load_rgba_row(pixels, y, 0, width, DeepDomain::Encoded, row);
    for (int x = 0; x < width; ++x) {
      if (row[static_cast<std::size_t>(x) * 4U + 3U] > 0.0F) {
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
      }
    }
  }
  if (right < left || (left == 0 && top == 0 && right == width - 1 && bottom == height - 1)) {
    return;
  }
  PixelBuffer cropped(right - left + 1, bottom - top + 1, pixels.format());
  const auto pixel_bytes = static_cast<std::size_t>(bytes_per_pixel(pixels.format()));
  for (int y = top; y <= bottom; ++y) {
    std::memcpy(cropped.pixel(0, y - top), std::as_const(pixels).pixel(left, y),
                static_cast<std::size_t>(right - left + 1) * pixel_bytes);
  }
  bounds.x += left;
  bounds.y += top;
  bounds.width = cropped.width();
  bounds.height = cropped.height();
  pixels = std::move(cropped);
}

// `source` reduced to width x height by averaging the source pixels under each result
// pixel. Color is weighted by alpha, so a transparent neighbour does not darken an edge.
[[nodiscard]] gmic::Image reduced_image(const gmic::Image& source, int width, int height) {
  gmic::Image out = gmic::make_image(width, height, source.channels);
  out.name = source.name;
  const int alpha_channel = source.channels == 4 ? 3 : source.channels == 2 ? 1 : -1;
  const int color_channels = alpha_channel >= 0 ? alpha_channel : source.channels;
  std::vector<double> sums(static_cast<std::size_t>(source.channels));
  for (int y = 0; y < height; ++y) {
    const int top = static_cast<int>(static_cast<std::int64_t>(y) * source.height / height);
    const int bottom =
        std::max(top + 1, static_cast<int>(static_cast<std::int64_t>(y + 1) * source.height / height));
    for (int x = 0; x < width; ++x) {
      const int left = static_cast<int>(static_cast<std::int64_t>(x) * source.width / width);
      const int right =
          std::max(left + 1, static_cast<int>(static_cast<std::int64_t>(x + 1) * source.width / width));
      std::fill(sums.begin(), sums.end(), 0.0);
      double weight_sum = 0.0;
      for (int sy = top; sy < bottom; ++sy) {
        for (int sx = left; sx < right; ++sx) {
          const double weight = alpha_channel >= 0 ? static_cast<double>(source.at(sx, sy, alpha_channel)) : 1.0;
          weight_sum += weight;
          for (int channel = 0; channel < color_channels; ++channel) {
            sums[static_cast<std::size_t>(channel)] += weight * static_cast<double>(source.at(sx, sy, channel));
          }
        }
      }
      const double count = static_cast<double>(bottom - top) * static_cast<double>(right - left);
      for (int channel = 0; channel < color_channels; ++channel) {
        out.at(x, y, channel) =
            weight_sum > 0.0 ? static_cast<float>(sums[static_cast<std::size_t>(channel)] / weight_sum) : 0.0F;
      }
      if (alpha_channel >= 0) {
        out.at(x, y, alpha_channel) = static_cast<float>(weight_sum / count);
      }
    }
  }
  return out;
}

// `source` enlarged to width x height, bilinear.
[[nodiscard]] gmic::Image enlarged_image(const gmic::Image& source, int width, int height) {
  gmic::Image out = gmic::make_image(width, height, source.channels);
  out.name = source.name;
  const double step_x = static_cast<double>(source.width) / static_cast<double>(width);
  const double step_y = static_cast<double>(source.height) / static_cast<double>(height);
  for (int y = 0; y < height; ++y) {
    const double fy = std::clamp((static_cast<double>(y) + 0.5) * step_y - 0.5, 0.0, static_cast<double>(source.height - 1));
    const int y0 = static_cast<int>(fy);
    const int y1 = std::min(y0 + 1, source.height - 1);
    const auto wy = static_cast<float>(fy - static_cast<double>(y0));
    for (int x = 0; x < width; ++x) {
      const double fx =
          std::clamp((static_cast<double>(x) + 0.5) * step_x - 0.5, 0.0, static_cast<double>(source.width - 1));
      const int x0 = static_cast<int>(fx);
      const int x1 = std::min(x0 + 1, source.width - 1);
      const auto wx = static_cast<float>(fx - static_cast<double>(x0));
      for (int channel = 0; channel < source.channels; ++channel) {
        const float upper = source.at(x0, y0, channel) * (1.0F - wx) + source.at(x1, y0, channel) * wx;
        const float lower = source.at(x0, y1, channel) * (1.0F - wx) + source.at(x1, y1, channel) * wx;
        out.at(x, y, channel) = upper * (1.0F - wy) + lower * wy;
      }
    }
  }
  return out;
}

// G'MIC's blend names (its 'blend' command) as Patchy modes; unknown ones are Normal.
[[nodiscard]] BlendMode blend_mode_from_gmic(const std::string& name) {
  static const std::pair<const char*, BlendMode> kModes[] = {
      {"multiply", BlendMode::Multiply},       {"screen", BlendMode::Screen},
      {"overlay", BlendMode::Overlay},         {"darken", BlendMode::Darken},
      {"lighten", BlendMode::Lighten},         {"dodge", BlendMode::ColorDodge},
      {"burn", BlendMode::ColorBurn},          {"hardlight", BlendMode::HardLight},
      {"softlight", BlendMode::SoftLight},     {"difference", BlendMode::Difference},
      {"exclusion", BlendMode::Exclusion},     {"hue", BlendMode::Hue},
      {"saturation", BlendMode::Saturation},   {"color", BlendMode::Color},
      {"value", BlendMode::Luminosity},        {"luminance", BlendMode::Luminosity},
      {"lightness", BlendMode::Luminosity},    {"add", BlendMode::LinearDodge},
      {"subtract", BlendMode::Subtract},       {"divide", BlendMode::Divide},
      {"linearburn", BlendMode::LinearBurn},   {"pinlight", BlendMode::PinLight},
      {"vividlight", BlendMode::VividLight},   {"linearlight", BlendMode::LinearLight},
      {"hardmix", BlendMode::HardMix},         {"dissolve", BlendMode::Dissolve},
  };
  for (const auto& [gmic_name, mode] : kModes) {
    if (name == gmic_name) {
      return mode;
    }
  }
  return BlendMode::Normal;
}

[[nodiscard]] QString gmic_text(const std::string& text) {
  return QString::fromUtf8(text.c_str(), static_cast<qsizetype>(text.size()));
}

// Pixels a run works through: each writable layer once when the filter runs on the
// layers one at a time, every chosen layer at document size when they are lined up.
[[nodiscard]] double gmic_input_pixels(const GmicRunInput& input) {
  double pixels = 0.0;
  for (const auto& layer : input.layers) {
    if (input.document_space) {
      pixels += static_cast<double>(input.document_width) * static_cast<double>(input.document_height);
    } else if (layer.writable) {
      pixels += static_cast<double>(layer.pixels->width()) * static_cast<double>(layer.pixels->height());
    }
  }
  return pixels;
}

// The float images a run holds at its peak, in MB: Patchy's planar copy of what it
// reads, the interpreter's own copy, and as much again for a result. A filter run on
// the layers one at a time needs room for the largest; lined-up layers all at once.
[[nodiscard]] std::int64_t gmic_run_memory_mb(const GmicRunInput& input) {
  std::int64_t samples = 0;
  if (input.document_space) {
    samples = static_cast<std::int64_t>(input.document_width) * input.document_height * 4 *
              static_cast<std::int64_t>(input.layers.size());
  } else {
    for (const auto& layer : input.layers) {
      if (layer.writable) {
        samples = std::max(samples, static_cast<std::int64_t>(layer.pixels->width()) * layer.pixels->height() *
                                        (layer.pixels->format().channels >= 4 ? 4 : 3));
      }
    }
  }
  const double scale = std::clamp(input.scale, 0.01, 1.0);
  return static_cast<std::int64_t>(static_cast<double>(samples) * scale * scale) *
         static_cast<std::int64_t>(sizeof(float)) * 3 / (1024 * 1024);
}

// The factor a draft of this run is reduced by: about a megapixel of input in all (or
// PATCHY_GMIC_DRAFT_PIXELS, for tests). 1 when the input is already near that size.
[[nodiscard]] double gmic_draft_scale(const GmicRunInput& input) {
  double budget = 1'000'000.0;
  if (const auto pixels = environment_variable("PATCHY_GMIC_DRAFT_PIXELS"); pixels.has_value()) {
    if (const auto value = std::atof(pixels->c_str()); value >= 1.0) {
      budget = value;
    }
  }
  const double pixels = gmic_input_pixels(input);
  return pixels <= budget * 1.5 ? 1.0 : std::sqrt(budget / pixels);
}

[[nodiscard]] double gmic_input_megapixels(const GmicRunInput& input) {
  return gmic_input_pixels(input) / 1'000'000.0;
}

// Whether a run can change anything a draft could show.
[[nodiscard]] bool gmic_has_writable_input(const GmicRunInput& input) {
  return std::any_of(input.layers.begin(), input.layers.end(),
                     [](const GmicLayerSnapshot& layer) { return layer.writable; });
}

// What a run may take: 60 percent of the installed memory, or PATCHY_GMIC_MEMORY_LIMIT_MB
// (tests). No limit where the platform does not report its memory.
[[nodiscard]] std::int64_t gmic_memory_budget_mb() {
  if (const auto limit = environment_variable("PATCHY_GMIC_MEMORY_LIMIT_MB"); limit.has_value()) {
    if (const auto value = std::atoll(limit->c_str()); value > 0) {
      return value;
    }
  }
  const auto installed = static_cast<std::int64_t>(total_physical_ram_mb());
  return installed > 0 ? installed * 6 / 10 : std::numeric_limits<std::int64_t>::max();
}

// The name a layer goes to G'MIC under. Filters read a layer's name from the `name(...)`
// field of the host's layer description (G'MIC's gui_layer_name); a bare name reads as
// "[unnamed]", so Channels to Layers named its layers "[unnamed] [green]". No `pos()`:
// a filter that echoes the name back would then pin a resized result to the corner
// instead of centring it.
[[nodiscard]] std::string gmic_input_name(const std::string& layer_name) {
  return "name(" + (layer_name.empty() ? std::string("Layer") : layer_name) + ")";
}

// Where an output image lands, in document pixels: at the position its name states
// (in the pixels of the image the filter ran on, from that image's corner), or centred
// on that image when it changed size without stating one.
[[nodiscard]] Rect gmic_output_bounds(const gmic::Image& image, const gmic::OutputLayerInfo& info, Rect frame,
                                      double scale) {
  Rect bounds{frame.x, frame.y, image.width, image.height};
  if (info.position.has_value()) {
    bounds.x += static_cast<int>(std::lround(info.position->first / scale));
    bounds.y += static_cast<int>(std::lround(info.position->second / scale));
  } else if (image.width != frame.width || image.height != frame.height) {
    // A frame grows evenly around the layer; an upscale stays centred on it.
    bounds.x += (frame.width - image.width) / 2;
    bounds.y += (frame.height - image.height) / 2;
  }
  return bounds;
}

// Turns what the filter returned for `layer` into its new pixels: back to the full frame
// for a draft, limited to the selection, cropped to its content when the layers were
// lined up. Kept in `render` only when it differs from the layer. False when it changes
// the layer's frame while a selection exists.
[[nodiscard]] bool add_gmic_layer_result(const GmicRunInput& input, const GmicLayerSnapshot& layer,
                                         gmic::Image& image, Rect frame, int run_width, int run_height,
                                         GmicRenderResult& render) {
  const double scale = std::clamp(input.scale, 0.01, 1.0);
  if (scale < 1.0) {
    // Back to the full frame: exactly, when the filter kept its image's size.
    const bool kept_size = image.width == run_width && image.height == run_height;
    const int width = kept_size ? frame.width : std::max(1, static_cast<int>(std::lround(image.width / scale)));
    const int height = kept_size ? frame.height : std::max(1, static_cast<int>(std::lround(image.height / scale)));
    image = enlarged_image(image, width, height);
  }
  const PixelBuffer& original = *layer.pixels;
  GmicLayerResult result;
  result.id = layer.id;
  result.original_bounds = layer.bounds;
  result.pixels = pixels_from_gmic_image(image, original.format());
  result.bounds = gmic_output_bounds(image, gmic::parse_output_layer_name(image.name), frame, scale);
  const bool same_frame = result.bounds.x == frame.x && result.bounds.y == frame.y &&
                          result.pixels.width() == frame.width && result.pixels.height() == frame.height;
  if (!same_frame && !input.selection.isEmpty()) {
    return false;
  }
  bool changed = true;
  if (same_frame) {
    // What the result is compared and blended against, in the same frame.
    const PixelBuffer embedded = input.document_space
                                     ? embed_in_document(original, layer.bounds, frame.width, frame.height)
                                     : PixelBuffer();
    const PixelBuffer& before = input.document_space ? embedded : original;
    if (!input.selection.isEmpty()) {
      const auto pixel_bytes = static_cast<std::size_t>(bytes_per_pixel(result.pixels.format()));
      // Restore unselected spans in one region walk, then feather by coverage.
      const auto outside = QRegion(to_qrect(frame)).subtracted(input.selection);
      for (const auto& rect : outside) {
        const auto local = rect.translated(-frame.x, -frame.y);
        for (int y = local.top(); y <= local.bottom(); ++y) {
          std::copy_n(before.pixel(local.left(), y), static_cast<std::size_t>(local.width()) * pixel_bytes,
                      result.pixels.pixel(local.left(), y));
        }
      }
      blend_by_selection_coverage(result.pixels, before, frame, input.selection);
    }
    changed = !pixel_buffers_equal(result.pixels, before);
  }
  if (input.document_space) {
    trim_to_content(result.pixels, result.bounds);
  }
  if (changed) {
    render.layers.push_back(std::move(result));
  }
  return true;
}

// A further output as a new layer below `anchor`, with the name, blend mode, opacity and
// position the filter gave it. Never part of a draft.
void add_gmic_new_layer(const GmicRunInput& input, const gmic::Image& image, Rect frame, LayerId anchor,
                        GmicRenderResult& render) {
  if (image.width <= 0 || image.height <= 0) {
    return;
  }
  const auto info = gmic::parse_output_layer_name(image.name);
  GmicNewLayer layer;
  layer.pixels = pixels_from_gmic_image(image, input.new_layer_format);
  layer.bounds = gmic_output_bounds(image, info, frame, 1.0);
  if (input.document_space) {
    trim_to_content(layer.pixels, layer.bounds);
  }
  layer.name = info.name.empty() ? input.filter_name : info.name;
  layer.blend_mode = blend_mode_from_gmic(info.blend_mode);
  layer.opacity = info.opacity.has_value() ? static_cast<float>(*info.opacity / 100.0) : 1.0F;
  layer.anchor = anchor;
  render.new_layers.push_back(std::move(layer));
}

// A filter made for one image, run on each writable chosen layer alone at its own size,
// as if it were applied to each in turn. The first image a run returns replaces that
// layer; the others become new layers below it (Ball's sphere over the original).
[[nodiscard]] GmicRenderResult render_gmic_each_layer(const GmicRunInput& input, gmic::RunControl* control) {
  GmicRenderResult render;
  const double scale = std::clamp(input.scale, 0.01, 1.0);
  const bool draft = scale < 1.0;
  const gmic::Engine engine(gmic_library().commands);
  bool have_status = false;
  for (std::size_t index = 0; index < input.layers.size(); ++index) {
    const auto& layer = input.layers[index];
    if (!layer.writable) {
      ++render.read_only_layers;
      continue;
    }
    const Rect frame{layer.bounds.x, layer.bounds.y, layer.pixels->width(), layer.pixels->height()};
    const int run_width = draft ? std::max(1, static_cast<int>(std::lround(frame.width * scale))) : frame.width;
    const int run_height = draft ? std::max(1, static_cast<int>(std::lround(frame.height * scale))) : frame.height;
    std::vector<gmic::Image> images;
    auto image = gmic_image_from_pixels(*layer.pixels);
    images.push_back(draft ? reduced_image(image, run_width, run_height) : std::move(image));
    // Hosts name their layers; several filters read the name.
    images.back().name = gmic_input_name(layer.name);
    const auto result = engine.run(input.command, images, control);
    if (result.aborted) {
      render.aborted = true;
      return render;
    }
    if (!result.ok) {
      render.error = result.error;
      return render;
    }
    // The controls follow the active layer's run.
    if (!have_status || input.active_input == index) {
      render.status = result.status;
      have_status = true;
    }
    if (images.empty() || images.front().width <= 0 || images.front().height <= 0) {
      return render;  // ok stays false with no message: the filter returned no image
    }
    if (!add_gmic_layer_result(input, layer, images.front(), frame, run_width, run_height, render)) {
      render.size_refused = true;
      return render;
    }
    for (std::size_t extra = 1; !draft && extra < images.size(); ++extra) {
      add_gmic_new_layer(input, images[extra], frame, layer.id, render);
    }
  }
  if (render.read_only_layers == input.layers.size()) {
    render.nothing_writable = true;
    return render;
  }
  render.ok = true;
  return render;
}

// A filter made for several layers, given them lined up on document-sized images. One
// that returns an image per layer it read worked on each: each writable layer takes the
// image in its place. One that returns fewer combined them: the first image replaces the
// active layer. Any other image becomes a new layer: below the lowest layer read when
// each layer took its own, below the active layer otherwise.
[[nodiscard]] GmicRenderResult render_gmic_lined_up(const GmicRunInput& input, gmic::RunControl* control) {
  GmicRenderResult render;
  const Rect frame{0, 0, input.document_width, input.document_height};
  const double scale = std::clamp(input.scale, 0.01, 1.0);
  const bool draft = scale < 1.0;
  const int run_width = draft ? std::max(1, static_cast<int>(std::lround(frame.width * scale))) : frame.width;
  const int run_height = draft ? std::max(1, static_cast<int>(std::lround(frame.height * scale))) : frame.height;
  std::vector<gmic::Image> images;
  images.reserve(input.layers.size());
  for (const auto& layer : input.layers) {
    auto image = gmic_document_image(*layer.pixels, layer.bounds, frame.width, frame.height);
    images.push_back(draft ? reduced_image(image, run_width, run_height) : std::move(image));
    images.back().name = gmic_input_name(layer.name);
  }
  const gmic::Engine engine(gmic_library().commands);
  const auto result = engine.run(input.command, images, control);
  render.aborted = result.aborted;
  render.status = result.status;
  if (!result.ok) {
    render.error = result.error;
    return render;
  }
  if (images.empty()) {
    return render;  // ok stays false with no message: the filter returned no image
  }
  const std::size_t input_count = input.layers.size();
  std::size_t first_new = 0;
  if (images.size() >= input_count) {
    for (std::size_t index = 0; index < input_count; ++index) {
      const auto& layer = input.layers[index];
      if (!layer.writable) {
        ++render.read_only_layers;
        continue;
      }
      if (images[index].width <= 0 || images[index].height <= 0) {
        continue;
      }
      if (!add_gmic_layer_result(input, layer, images[index], frame, run_width, run_height, render)) {
        render.size_refused = true;
        return render;
      }
    }
    first_new = input_count;
  } else if (input.active_input.has_value() && input.layers[*input.active_input].writable &&
             images.front().width > 0 && images.front().height > 0) {
    if (!add_gmic_layer_result(input, input.layers[*input.active_input], images.front(), frame, run_width,
                               run_height, render)) {
      render.size_refused = true;
      return render;
    }
    render.merged = input_count > 1;
    first_new = 1;
  }
  const LayerId anchor = images.size() >= input_count ? input.layers.back().id : input.active;
  for (std::size_t index = first_new; !draft && index < images.size(); ++index) {
    add_gmic_new_layer(input, images[index], frame, anchor, render);
  }
  if (!render.changed() && render.read_only_layers == input_count) {
    render.nothing_writable = true;
    return render;
  }
  render.ok = true;
  return render;
}

[[nodiscard]] GmicRenderResult render_gmic_filter_unguarded(const GmicRunInput& input, gmic::RunControl* control) {
  if (input.layers.empty()) {
    GmicRenderResult render;
    render.no_input = true;
    return render;
  }
  if (const auto needed_mb = gmic_run_memory_mb(input); needed_mb > gmic_memory_budget_mb()) {
    GmicRenderResult render;
    render.memory_refused_mb = needed_mb;
    return render;
  }
  return input.document_space ? render_gmic_lined_up(input, control) : render_gmic_each_layer(input, control);
}

// The whole run: layers to float, the engine, float back to layer formats, and
// the selection. Thread-safe; touches no widget and translates nothing. Never
// throws: it runs on worker threads, where an escaped exception (a float copy
// of a large document can exhaust memory) would end the process.
[[nodiscard]] GmicRenderResult render_gmic_filter(const GmicRunInput& input, gmic::RunControl* control) {
  try {
    return render_gmic_filter_unguarded(input, control);
  } catch (const std::exception& error) {
    GmicRenderResult failed;
    failed.error = error.what();
    return failed;
  } catch (...) {
    GmicRenderResult failed;
    failed.error = "Unknown G'MIC failure.";
    return failed;
  }
}

// A filter may change a layer with editable RGB pixels that are not locked. Text, shape
// and Smart Object layers are read as their pixels and left as they are.
[[nodiscard]] bool gmic_layer_writable(const Document& document, const Layer& layer) {
  return editable_rgb_layer_any_depth(&layer) && !layer_is_smart_object(layer) && !layer_is_text(layer) &&
         !layer_is_vector_shape(layer) &&
         (layer_effective_lock_flags(document.layers(), layer.id()) & kLayerLockImagePixels) == kLayerLockNone;
}

// Bottom to top, groups opened: every layer that can be shown as RGB pixels.
void collect_gmic_layer_snapshots(const Document& document, const std::vector<Layer>& layers, bool parents_visible,
                                  std::vector<GmicLayerSnapshot>& out) {
  for (const auto& layer : layers) {
    const bool visible = parents_visible && layer.visible();
    if (layer.kind() == LayerKind::Group) {
      collect_gmic_layer_snapshots(document, layer.children(), visible, out);
    } else if (layer.kind() != LayerKind::Adjustment && layer.pixels().width() > 0 && layer.pixels().height() > 0 &&
               layer.pixels().format().color_mode == ColorMode::RGB && layer.pixels().format().channels >= 3) {
      out.push_back({layer.id(), std::make_shared<const PixelBuffer>(layer.pixels()), layer.bounds(), layer.name(),
                     visible, gmic_layer_writable(document, layer)});
    }
  }
}

// Every layer a filter could read, top of the stack first. Shared, copy-on-write
// buffers: this copies no pixels.
[[nodiscard]] std::vector<GmicLayerSnapshot> gmic_layer_snapshots(const Document& document) {
  std::vector<GmicLayerSnapshot> snapshots;
  collect_gmic_layer_snapshots(document, document.layers(), true, snapshots);
  std::reverse(snapshots.begin(), snapshots.end());
  return snapshots;
}

// Picks the layers `mode` names from `snapshots` (top of the stack first) and where the
// active layer is among them.
void set_gmic_input_layers(GmicRunInput& input, const std::vector<GmicLayerSnapshot>& snapshots, LayerId active,
                           char mode) {
  const auto active_at =
      std::find_if(snapshots.begin(), snapshots.end(),
                   [active](const GmicLayerSnapshot& snapshot) { return snapshot.id == active; });
  input.layers.clear();
  input.active_input.reset();
  input.active = active;
  for (auto it = snapshots.begin(); it != snapshots.end(); ++it) {
    const bool is_active = it == active_at;
    bool wanted = false;
    switch (mode) {
      case '*': wanted = true; break;
      case 'v': wanted = it->visible; break;
      case 'i': wanted = !it->visible; break;
      case '+': wanted = is_active || (active_at != snapshots.end() && it == active_at + 1); break;
      case '-': wanted = is_active || (active_at != snapshots.end() && it + 1 == active_at); break;
      default: wanted = is_active; break;
    }
    if (wanted) {
      if (is_active) {
        input.active_input = input.layers.size();
      }
      input.layers.push_back(*it);
    }
  }
}

// A layer named Background (any case) stays at the bottom of the stack: Photoshop keeps it
// there, and Patchy's PSD reader takes a file whose top layer is a full-canvas Background
// for a legacy top-to-bottom one and reverses it.
[[nodiscard]] bool gmic_keeps_at_bottom(const Document& document, LayerId id) {
  const auto* layer = document.find_layer(id);
  if (layer == nullptr) {
    return false;
  }
  std::string name = layer->name();
  std::transform(name.begin(), name.end(), name.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return name == "background";
}

// Inserts a run's further outputs as new layers in G'MIC's order (top of the stack first):
// each below the layer it belongs to, several for one layer each below the last. Under a
// Background layer they go above it instead, still in that order among themselves. The
// pixels are shared, copy-on-write: the preview shows the very layers OK will add.
// Returns their ids.
std::vector<LayerId> insert_gmic_new_layers(Document& document, const std::vector<GmicNewLayer>& outputs) {
  std::map<LayerId, LayerId> lowest_added;  // anchor -> the last layer inserted below it
  std::vector<LayerId> added;
  added.reserve(outputs.size());
  for (const auto& output : outputs) {
    Layer added_layer(document.allocate_layer_id(), output.name, output.pixels);
    added_layer.set_bounds(output.bounds);
    added_layer.set_blend_mode(output.blend_mode);
    added_layer.set_opacity(output.opacity);
    added_layer.set_visible(output.visible);
    const auto added_id = added_layer.id();
    if (gmic_keeps_at_bottom(std::as_const(document), output.anchor)) {
      // Each goes directly above the Background, under the ones added before it.
      insert_layer_after_anchor(document, std::move(added_layer), output.anchor);
    } else {
      const auto found = lowest_added.find(output.anchor);
      insert_layer_below_anchor(document, std::move(added_layer),
                                found != lowest_added.end() ? found->second : output.anchor);
      lowest_added[output.anchor] = added_id;
    }
    added.push_back(added_id);
  }
  return added;
}

// Puts a finished run into the document: each changed layer's new pixels, then the new
// layers. Returns how many layers were added. The caller owns undo and refresh.
std::size_t write_gmic_render(Document& document, GmicRenderResult& render) {
  for (auto& result : render.layers) {
    if (auto* layer = document.find_layer(result.id); layer != nullptr) {
      const auto bounds = result.bounds;
      set_layer_pixels_with_bounds(*layer, std::move(result.pixels), bounds);
    }
  }
  return insert_gmic_new_layers(document, render.new_layers).size();
}

// Whether a filter made for several layers gets the chosen ones together, lined up on
// the document. A filter made for one image runs on each chosen layer alone instead.
[[nodiscard]] bool gmic_reads_layers_together(const gmic::Filter& filter, char input_mode) {
  return input_mode != '.' && filter.input_mode != '.';
}

// Everything a run reads, gathered on the UI thread (shared buffers, no pixel copies).
[[nodiscard]] GmicRunInput make_gmic_run_input(const std::vector<GmicLayerSnapshot>& snapshots, LayerId active,
                                               const Document& document, const gmic::Filter& filter,
                                               const std::vector<std::string>& values, char input_mode,
                                               FilterSelection selection) {
  GmicRunInput input;
  input.document_width = document.width();
  input.document_height = document.height();
  set_gmic_input_layers(input, snapshots, active, input_mode);
  // One layer lines up with nothing: it runs at its own size like any single layer.
  input.document_space = gmic_reads_layers_together(filter, input_mode) && input.layers.size() > 1;
  input.selection = std::move(selection);
  input.command = gmic::command_line(filter, values, false);
  input.filter_name = filter.name;
  input.new_layer_format = with_bit_depth(PixelFormat::rgba8(), document.color_state().bit_depth);
  return input;
}

// Why a run that is neither ok nor aborted produced nothing, for the user.
[[nodiscard]] QString gmic_failure_text(const GmicRenderResult& render, const QString& name) {
  if (render.size_refused) {
    return MainWindow::tr("%1 changes the layer size. Deselect before applying it.").arg(name);
  }
  if (render.no_input) {
    return MainWindow::tr("No layer matches the chosen input layers.");
  }
  if (render.nothing_writable) {
    return MainWindow::tr("None of the chosen layers can be changed. Locked, text, shape and Smart Object layers "
                          "stay as they are.");
  }
  if (render.memory_refused_mb > 0) {
    return MainWindow::tr("%1 needs about %2 MB of memory for these layers, more than this computer can spare. "
                          "Use fewer input layers or a smaller image.")
        .arg(name)
        .arg(static_cast<qlonglong>(render.memory_refused_mb));
  }
  return render.error.empty() ? MainWindow::tr("The filter returned no image.") : gmic_text(render.error);
}

// A refusal the user fixes by changing a setting, as opposed to a filter that failed.
[[nodiscard]] bool gmic_run_was_refused(const GmicRenderResult& render) {
  return render.size_refused || render.no_input || render.nothing_writable || render.memory_refused_mb > 0;
}

// The result that says the most about a layer's size: the active layer's, else the first.
[[nodiscard]] const GmicLayerResult* gmic_main_result(const GmicRenderResult& render, LayerId active) {
  const auto found = std::find_if(render.layers.begin(), render.layers.end(),
                                  [active](const GmicLayerResult& result) { return result.id == active; });
  if (found != render.layers.end()) {
    return &*found;
  }
  return render.layers.empty() ? nullptr : &render.layers.front();
}

// What applying a previewed result will do that the preview does not make obvious, one
// sentence each: several layers changed or combined, a layer resized, layers added,
// chosen layers that stay as they are.
[[nodiscard]] QStringList gmic_result_notes(const GmicRenderResult& render, LayerId active, int document_width,
                                            int document_height) {
  QStringList notes;
  if (render.merged) {
    notes << MainWindow::tr("Combines the chosen layers into the active layer. The other layers stay as they are.");
  } else if (render.layers.size() > 1) {
    notes << MainWindow::tr("Changes %n layer(s).", nullptr, static_cast<int>(render.layers.size()));
  }
  if (const auto* result = gmic_main_result(render, active);
      result != nullptr && (result->bounds.width != result->original_bounds.width ||
                            result->bounds.height != result->original_bounds.height)) {
    notes << MainWindow::tr("Changes the layer size from %1 x %2 to %3 x %4 pixels.")
                 .arg(result->original_bounds.width)
                 .arg(result->original_bounds.height)
                 .arg(result->bounds.width)
                 .arg(result->bounds.height);
    if (result->bounds.x < 0 || result->bounds.y < 0 || result->bounds.x + result->bounds.width > document_width ||
        result->bounds.y + result->bounds.height > document_height) {
      notes << MainWindow::tr("Part of the layer will lie outside the canvas. It is kept, and the Move tool "
                              "brings it into view.");
    }
  }
  if (!render.new_layers.empty()) {
    notes << MainWindow::tr("Adds %n new layer(s). The preview already shows them on the canvas and in the Layers "
                            "panel.",
                            nullptr, static_cast<int>(render.new_layers.size()));
  }
  if (render.read_only_layers > 0) {
    notes << MainWindow::tr("%n of the chosen layers cannot be changed (locked, text, shape or Smart Object) and "
                            "stay as they are.",
                            nullptr, static_cast<int>(render.read_only_layers));
  }
  return notes;
}

[[nodiscard]] bool same_choice(const std::optional<GmicFilterChoice>& lhs, const GmicFilterChoice& rhs) {
  return lhs.has_value() && lhs->filter_id == rhs.filter_id && lhs->values == rhs.values &&
         lhs->input_mode == rhs.input_mode;
}

// Keeps the window alive under a cancellable progress box until `ready` (which waits a
// few milliseconds itself: a bare processEvents loop spins a core) reports the run over.
// Cancel, or `keep_running` returning false, sets the run's abort flag; the wait goes on
// until the run has actually stopped.
void wait_for_gmic_run(QWidget* parent, CanvasWidget* canvas, const QString& name, gmic::RunControl& control,
                       const std::function<bool()>& ready, const std::function<bool()>& keep_running = {}) {
  const QPointer<CanvasWidget> canvas_guard(canvas);
  if (canvas_guard != nullptr) {
    canvas_guard->begin_processing_operation();
  }
  const auto finish_processing = qScopeGuard([canvas_guard] {
    if (canvas_guard != nullptr) {
      canvas_guard->end_processing_operation();
    }
  });
  QProgressDialog progress(MainWindow::tr("Applying %1...").arg(name), MainWindow::tr("Cancel"), 0, 100, parent);
  progress.setObjectName(QStringLiteral("gmicProgressDialog"));
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(kFilterProgressMinimumDurationMs);
  remember_dialog_position(progress);
  while (!ready()) {
    QApplication::processEvents(QEventLoop::AllEvents, 30);
    if (canvas_guard != nullptr) {
      canvas_guard->tick_processing_operation();
    }
    const float percent = control.progress.load();
    // G'MIC reports progress (0..100) only from some filters; the bar stays busy otherwise.
    progress.setRange(0, percent >= 0.0F ? 100 : 0);
    if (percent >= 0.0F) {
      // 100 would close the box before the result is in the document.
      progress.setValue(std::clamp(static_cast<int>(std::lround(percent)), 0, 99));
    }
    if (progress.wasCanceled() || (keep_running && !keep_running())) {
      control.abort.store(true);
    }
  }
  progress.reset();
}

}  // namespace

std::optional<LayerId> MainWindow::gmic_target_layer(const QString& operation_name) {
  if (!has_active_document() || canvas_ == nullptr) {
    return std::nullopt;
  }
  if (canvas_->quick_mask_active()) {
    show_status_error(tr("%1 is unavailable in Quick Mask mode").arg(operation_name));
    return std::nullopt;
  }
  auto& doc = document();
  select_only_layer_if_none_active();
  const auto active = doc.active_layer_id();
  if (!active.has_value()) {
    // No layer selected (and more than one to choose from): say so instead of doing nothing.
    show_status_error(tr("Select an editable RGB pixel layer"));
    return std::nullopt;
  }
  const auto& read_only_doc = std::as_const(doc);
  const auto* layer = read_only_doc.find_layer(*active);
  if (!editable_rgb_layer_any_depth(layer)) {
    show_status_error(tr("Select an editable RGB pixel layer"));
    return std::nullopt;
  }
  if (layer_is_smart_object(*layer)) {
    show_status_error(tr("Rasterize the Smart Object before using %1").arg(operation_name));
    return std::nullopt;
  }
  if (layer_id_locks_image_pixels(*active)) {
    show_status_error(tr("Layer pixels are locked."));
    return std::nullopt;
  }
  // A G'MIC filter only ever sees pixels, so text and shape layers are rasterized first.
  if (!prompt_rasterize_procedural_layer(*active, operation_name, false)) {
    return std::nullopt;
  }
  layer = read_only_doc.find_layer(*active);
  if (!editable_rgb_layer_any_depth(layer) || layer->pixels().width() <= 0 || layer->pixels().height() <= 0) {
    show_status_error(tr("Select an editable RGB pixel layer"));
    return std::nullopt;
  }
  return active;
}

GmicRenderResult MainWindow::render_gmic_with_progress(const QString& name, const GmicRunInput& input,
                                                       const std::function<bool()>& keep_running) {
  // The run blocks until the filter ends, so it happens on a worker that this
  // frame joins before returning; the event loop keeps the progress box alive.
  gmic::RunControl control;
  GmicRenderResult render;
  std::promise<void> finished_promise;
  const auto finished = finished_promise.get_future();
  std::thread worker([&] {
    render = render_gmic_filter(input, &control);
    finished_promise.set_value();
  });
  // Whatever leaves this frame, the worker is stopped and joined first.
  const auto join_worker = qScopeGuard([&] {
    control.abort.store(true);
    if (worker.joinable()) {
      worker.join();
    }
  });
  wait_for_gmic_run(
      this, canvas_, name, control,
      [&finished] { return finished.wait_for(std::chrono::milliseconds(15)) == std::future_status::ready; },
      keep_running);
  worker.join();
  return render;
}

bool MainWindow::commit_gmic_render(LayerId active, const QString& name, GmicRenderResult& render) {
  if (render.aborted) {
    statusBar()->showMessage(tr("Cancelled %1").arg(name));
    return false;
  }
  if (gmic_run_was_refused(render) && render.memory_refused_mb == 0) {
    show_status_error(gmic_failure_text(render, name));
    return false;
  }
  if (!render.ok) {
    show_critical_message(this, tr("G'MIC Filter Library"),
                          tr("%1 could not be applied.\n\n%2").arg(name, gmic_failure_text(render, name)),
                          QStringLiteral("gmicErrorMessageBox"));
    return false;
  }
  if (!render.changed()) {
    statusBar()->showMessage(tr("%1 made no changes").arg(name));
    return true;
  }
  push_undo_snapshot(tr("Filter: %1").arg(name));
  auto& doc = document();
  const auto changed_layers = render.layers.size();
  const auto* main_result = gmic_main_result(render, active);
  const bool resized = main_result != nullptr && (main_result->bounds.width != main_result->original_bounds.width ||
                                                  main_result->bounds.height != main_result->original_bounds.height);
  const Rect new_size = main_result != nullptr ? main_result->bounds : Rect{};
  QRect dirty;
  for (const auto& result : render.layers) {
    dirty = dirty.united(to_qrect(result.original_bounds)).united(to_qrect(result.bounds));
  }
  const auto added = write_gmic_render(doc, render);
  if (added > 0) {
    if (doc.find_layer(active) != nullptr) {
      doc.set_active_layer(active);
    }
    refresh_layer_list();
    refresh_layer_controls();
    canvas_->document_changed();
  } else {
    canvas_->document_changed(dirty);
  }
  schedule_palette_compliance_check();
  QString message;
  if (changed_layers > 1) {
    message = added > 0 ? tr("Applied %1 to %n layer(s) (new layers: %2)", nullptr, static_cast<int>(changed_layers))
                              .arg(name)
                              .arg(static_cast<qulonglong>(added))
                        : tr("Applied %1 to %n layer(s)", nullptr, static_cast<int>(changed_layers)).arg(name);
  } else if (added > 0) {
    message = tr("Applied %1 (new layers: %2)").arg(name).arg(static_cast<qulonglong>(added));
  } else if (resized) {
    message = tr("Applied %1. The layer is now %2 x %3 pixels.").arg(name).arg(new_size.width).arg(new_size.height);
  } else {
    message = tr("Applied %1").arg(name);
  }
  statusBar()->showMessage(message);
  return true;
}

bool MainWindow::apply_gmic_filter(const std::string& filter_id, const std::vector<std::string>& values,
                                   char input_mode) {
  if (!has_active_document() || canvas_ == nullptr) {
    return false;
  }
  const auto* filter = gmic_filters_available() ? gmic_library().catalog.find(filter_id) : nullptr;
  if (filter == nullptr) {
    show_status_error(tr("This G'MIC filter is not available"));
    return false;
  }
  const QString name = gmic_text(filter->name);
  const auto active = gmic_target_layer(name);
  if (!active.has_value()) {
    return false;
  }
  const auto& doc = std::as_const(document());
  const char mode = input_mode != 0 ? input_mode : filter->input_mode;
  const auto input = make_gmic_run_input(gmic_layer_snapshots(doc), *active, doc, *filter, values, mode,
                                         filter_selection(*canvas_));
  auto render = render_gmic_with_progress(name, input);
  return commit_gmic_render(*active, name, render);
}

MainWindow::GmicApplyStatus MainWindow::apply_gmic_filter_to_layer(DocumentSession& session, CanvasWidget* canvas,
                                                                   LayerId layer_id, const std::string& filter_id,
                                                                   const std::vector<std::string>& values,
                                                                   char input_mode, bool show_progress,
                                                                   const std::function<void()>& before_write,
                                                                   QString* error, std::size_t* added_layers,
                                                                   const std::function<bool()>& keep_running) {
  const auto fail = [error](const QString& message) {
    if (error != nullptr) {
      *error = message;
    }
    return GmicApplyStatus::Error;
  };
  const auto* filter = gmic_filters_available() ? gmic_library().catalog.find(filter_id) : nullptr;
  if (filter == nullptr) {
    return fail(tr("This G'MIC filter is not available"));
  }
  const QString name = gmic_text(filter->name);
  const auto& doc = std::as_const(session.document);
  const auto* layer = doc.find_layer(layer_id);
  if (!editable_rgb_layer_any_depth(layer) || layer->pixels().width() <= 0 || layer->pixels().height() <= 0) {
    return fail(tr("Select an editable RGB pixel layer"));
  }
  if (layer_is_smart_object(*layer)) {
    return fail(tr("Rasterize the Smart Object before using %1").arg(name));
  }
  // The same gates as the menu command, as refusals: a script gets no prompt.
  if (layer_is_text(*layer) || layer_is_vector_shape(*layer)) {
    return fail(tr("Rasterize Text, Smart Object, and Shape layers before editing their pixels"));
  }
  if ((layer_effective_lock_flags(doc.layers(), layer_id) & kLayerLockImagePixels) != kLayerLockNone) {
    return fail(tr("Layer pixels are locked."));
  }
  if (canvas != nullptr && canvas->quick_mask_active()) {
    return fail(tr("%1 is unavailable in Quick Mask mode").arg(name));
  }
  const char mode = input_mode != 0 ? input_mode : filter->input_mode;
  const auto input = make_gmic_run_input(gmic_layer_snapshots(doc), layer_id, doc, *filter, values, mode,
                                         canvas != nullptr ? filter_selection(*canvas) : FilterSelection{});
  GmicRenderResult render;
  if (show_progress && canvas_ != nullptr) {
    render = render_gmic_with_progress(name, input, keep_running);
  } else {
    // No progress box (unattended runs): the same worker, with the event loop kept
    // alive for repaints only.
    gmic::RunControl control;
    std::promise<void> finished_promise;
    const auto finished = finished_promise.get_future();
    std::thread worker([&] {
      render = render_gmic_filter(input, &control);
      finished_promise.set_value();
    });
    const auto join_worker = qScopeGuard([&] {
      control.abort.store(true);
      if (worker.joinable()) {
        worker.join();
      }
    });
    while (finished.wait_for(std::chrono::milliseconds(15)) != std::future_status::ready) {
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
      if (keep_running && !keep_running()) {
        control.abort.store(true);
      }
    }
    worker.join();
  }
  if (render.aborted) {
    return GmicApplyStatus::Cancelled;
  }
  if (!render.ok) {
    return fail(gmic_failure_text(render, name));
  }
  if (!render.changed()) {
    return GmicApplyStatus::NoChange;
  }
  if (before_write) {
    before_write();
  }
  if (session.document.find_layer(layer_id) == nullptr) {
    return fail(tr("Select an editable RGB pixel layer"));
  }
  const auto added = write_gmic_render(session.document, render);
  if (added_layers != nullptr) {
    *added_layers = added;
  }
  return GmicApplyStatus::Applied;
}

void MainWindow::gmic_filter_library_dialog() {
  if (!has_active_document() || canvas_ == nullptr || !gmic_filters_available()) {
    return;
  }
  const QString library_name = tr("G'MIC Filter Library");
  const auto active = gmic_target_layer(library_name);
  if (!active.has_value()) {
    return;
  }
  auto& doc = document();
  const auto* source_layer = std::as_const(doc).find_layer(*active);
  if (source_layer == nullptr) {
    return;
  }
  const auto& catalog = gmic_library().catalog;
  const auto bounds = source_layer->bounds();
  const auto selection = filter_selection(*canvas_);
  // Every layer a filter could read or change, as it is before any preview touches it.
  // The dialog locks edits, so only its own previews change these layers while it is open.
  const auto snapshots = std::make_shared<const std::vector<GmicLayerSnapshot>>(gmic_layer_snapshots(doc));

  // Built on the UI thread per request: the layers a choice reads are shared,
  // copy-on-write buffers, so this copies no pixels.
  const auto run_input = [this, active, snapshots, selection](const gmic::Filter& filter,
                                                               const GmicFilterChoice& choice) {
    return make_gmic_run_input(*snapshots, *active, std::as_const(document()), filter, choice.values,
                               choice.input_mode, selection);
  };

  GmicFilterLibraryDialog dialog(catalog, gmic_text(gmic::engine_version()), this);
  const QPointer<GmicFilterLibraryDialog> dialog_guard(&dialog);

  // What the canvas currently shows for the dialog, confined to the UI thread.
  struct Shown {
    std::shared_ptr<gmic::RunControl> running;       // the newest run, asked to stop when it goes stale
    std::optional<GmicFilterChoice> running_choice;  // the settings that run is for
    std::optional<GmicFilterChoice> choice;          // what `render` is the result of
    std::shared_ptr<GmicRenderResult> render;        // a full-quality result only, never a draft
    // The newest result to show, draft or full, and its settings. Kept while Preview is
    // off, so turning it back on shows it again without a new run.
    std::shared_ptr<GmicRenderResult> preview;
    std::optional<GmicFilterChoice> preview_choice;
    // The layers showing a preview now, and the bounds each shows.
    std::map<LayerId, Rect> layers;
    // The layers a result adds, inserted while it is shown and removed before anything
    // else touches the document (a commit inserts its own).
    std::vector<LayerId> added;
    // The full-quality run in flight: OK for the same settings waits for it instead of
    // starting over.
    std::optional<GmicFilterChoice> full_choice;
    std::shared_ptr<gmic::RunControl> full_control;
    std::shared_future<std::shared_ptr<GmicRenderResult>> full_result;
    int live_runs{0};     // runs still working, the stale ones that are stopping included
    bool enabled{true};   // the Preview box
  };
  auto shown = std::make_shared<Shown>();
  auto preview_state = std::make_shared<AsyncPixelPreviewState<GmicPreviewRequest>>();
  // A stale run stops at the interpreter's next check, which can take seconds; the newest
  // settings start at once beside it. Beyond this many runs a new request waits.
  constexpr int kMaxLivePreviewRuns = 2;

  const QPointer<MainWindow> self(this);
  // Puts the layers back that show a preview and are not in `keep`, and adds the area
  // they covered to `dirty`.
  const auto restore_layers = [self, snapshots, shown](const std::set<LayerId>& keep, QRect& dirty) {
    for (auto it = shown->layers.begin(); it != shown->layers.end();) {
      if (keep.count(it->first) > 0) {
        ++it;
        continue;
      }
      const auto snapshot =
          std::find_if(snapshots->begin(), snapshots->end(),
                       [id = it->first](const GmicLayerSnapshot& candidate) { return candidate.id == id; });
      if (auto* layer = self->document().find_layer(it->first); layer != nullptr && snapshot != snapshots->end()) {
        set_layer_pixels_preserving_origin(*layer, *snapshot->pixels, snapshot->bounds);
        dirty = dirty.united(to_qrect(it->second)).united(to_qrect(snapshot->bounds));
      }
      it = shown->layers.erase(it);
    }
  };
  // Replaces the previewed new layers with `outputs` (none puts the document's layer stack
  // back), keeping the active layer active. The Layers panel lists them while they show;
  // it is disabled while the dialog is open, so nobody can select or edit one.
  const auto show_added_layers = [self, shown, active](const std::vector<GmicNewLayer>& outputs) {
    if (self == nullptr || (shown->added.empty() && outputs.empty())) {
      return;
    }
    auto& document = self->document();
    // Eyes the user turned off on the previewed layers stay off when the next result
    // brings as many layers (a slider moved, not a different filter).
    std::vector<bool> eyes;
    for (const auto id : shown->added) {
      const auto* layer = std::as_const(document).find_layer(id);
      eyes.push_back(layer == nullptr || layer->visible());
      (void)document.remove_layer(id);
    }
    shown->added = insert_gmic_new_layers(document, outputs);
    if (eyes.size() == shown->added.size()) {
      for (std::size_t index = 0; index < eyes.size(); ++index) {
        if (auto* layer = document.find_layer(shown->added[index]); layer != nullptr) {
          layer->set_visible(eyes[index]);
        }
      }
    }
    if (document.find_layer(*active) != nullptr) {
      document.set_active_layer(*active);
    }
    self->refresh_layer_list();
    self->refresh_layer_controls();
    if (self->canvas_ != nullptr) {
      self->canvas_->document_changed();
    }
  };
  const auto put_on_layer = [self, shown, restore_layers, show_added_layers](const GmicRenderResult& render) {
    if (self == nullptr) {
      return;
    }
    show_added_layers(render.new_layers);
    std::set<LayerId> changed;
    for (const auto& result : render.layers) {
      changed.insert(result.id);
    }
    QRect dirty;
    restore_layers(changed, dirty);
    for (const auto& result : render.layers) {
      if (auto* layer = self->document().find_layer(result.id); layer != nullptr) {
        set_layer_pixels_with_bounds(*layer, PixelBuffer(result.pixels), result.bounds);
        const auto before = shown->layers.find(result.id);
        dirty = dirty.united(to_qrect(before != shown->layers.end() ? before->second : result.original_bounds))
                    .united(to_qrect(result.bounds));
        shown->layers[result.id] = result.bounds;
      }
    }
    if (self->canvas_ != nullptr && !dirty.isEmpty()) {
      self->canvas_->document_changed(dirty);
    }
  };
  const auto show_original_pixels = [self, restore_layers, show_added_layers] {
    if (self == nullptr) {
      return;
    }
    show_added_layers({});
    QRect dirty;
    restore_layers({}, dirty);
    if (self->canvas_ != nullptr && !dirty.isEmpty()) {
      self->canvas_->document_changed(dirty);
    }
  };
  const auto show_original = [show_original_pixels, shown] {
    show_original_pixels();
    shown->choice.reset();
    shown->render.reset();
    shown->preview.reset();
    shown->preview_choice.reset();
  };

  // A preview on a large image runs twice: a draft on a reduced copy, shown within about
  // a second, then the full-quality run that replaces it. Small images, and filters this
  // session has measured as fast, skip the draft.
  enum class Stage { Draft, Full };
  using Launch = std::function<void(Stage, const GmicFilterChoice&, std::uint64_t, bool)>;
  const auto launch = std::make_shared<Launch>();
  const std::weak_ptr<Launch> weak_launch = launch;
  *launch = [this, preview_state, shown, put_on_layer, show_original, run_input, dialog_guard, weak_launch,
             &catalog](Stage stage, const GmicFilterChoice& choice, std::uint64_t generation, bool after_draft) {
    const auto* filter = catalog.find(choice.filter_id);
    if (filter == nullptr) {
      return;
    }
    ++shown->live_runs;
    preview_state->in_flight = true;
    auto control = std::make_shared<gmic::RunControl>();
    shown->running = control;
    shown->running_choice = choice;
    auto input = run_input(*filter, choice);
    const double megapixels = gmic_input_megapixels(input);
    if (stage == Stage::Draft) {
      input.scale = gmic_draft_scale(input);
    }
    auto promise = std::make_shared<std::promise<std::shared_ptr<GmicRenderResult>>>();
    if (stage == Stage::Full) {
      shown->full_choice = choice;
      shown->full_control = control;
      shown->full_result = promise->get_future().share();
    }
    const QString name = gmic_text(filter->name);
    const LayerId active_id = input.active;
    const int document_width = input.document_width;
    const int document_height = input.document_height;
    auto* app = QCoreApplication::instance();
    const auto window = QPointer<MainWindow>(this);
    if (canvas_ != nullptr) {
      canvas_->begin_preview_render();
      canvas_->set_preview_render_refining(stage == Stage::Full && after_draft);
    }
    run_tracked_background_worker([app, window, preview_state, shown, put_on_layer, show_original, dialog_guard,
                                   generation, control, choice, name, stage, megapixels, promise, weak_launch,
                                   active_id, document_width, document_height, input = std::move(input)] {
      const auto started = std::chrono::steady_clock::now();
      auto render = std::make_shared<GmicRenderResult>(render_gmic_filter(input, control.get()));
      const double elapsed_ms =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
      promise->set_value(render);
      if (app == nullptr) {
        return;
      }
      QMetaObject::invokeMethod(
          app,
          [window, preview_state, shown, put_on_layer, show_original, dialog_guard, generation, control, choice,
           name, stage, megapixels, elapsed_ms, weak_launch, active_id, document_width, document_height, render] {
            --shown->live_runs;
            preview_state->in_flight = shown->live_runs > 0;
            if (shown->running == control) {
              shown->running.reset();
              shown->running_choice.reset();
            }
            if (shown->full_control == control) {
              shown->full_control.reset();
              shown->full_choice.reset();
              shown->full_result = {};
            }
            if (window != nullptr && window->canvas_ != nullptr) {
              window->canvas_->end_preview_render();
            }
            if (window != nullptr && stage == Stage::Full && render->ok && megapixels > 0.0) {
              window->gmic_full_ms_per_megapixel_[choice.filter_id] = elapsed_ms / megapixels;
            }
            const bool current = !preview_state->closed && !preview_state->pending.has_value() &&
                                 generation == preview_state->generation && window != nullptr;
            // The dialog still shows exactly what this run was given: no edit is waiting in
            // the coalescing delay. Only then may what the run hands back (new control values,
            // released buttons) be taken as the settings the shown result belongs to.
            const bool untouched = current && dialog_guard != nullptr && same_choice(dialog_guard->choice(), choice);
            if (stage == Stage::Draft) {
              if (current && !render->aborted) {
                // A draft that failed shows nothing; the full run gives the verdict.
                GmicFilterChoice next = choice;
                if (render->ok) {
                  if (untouched) {
                    // The values the draft returned are the ones the full run continues with.
                    dialog_guard->apply_status(choice.filter_id, render->status);
                    if (const auto updated = dialog_guard->choice();
                        updated.has_value() && updated->filter_id == choice.filter_id) {
                      next = *updated;
                    }
                  }
                  // Not a result OK may commit, but the one to show until the full run lands.
                  shown->render.reset();
                  shown->choice.reset();
                  shown->preview = render;
                  shown->preview_choice = next;
                  if (shown->enabled) {
                    put_on_layer(*render);
                    ++window->gmic_draft_previews_shown_;
                    if (const auto* main = gmic_main_result(*render, active_id); main != nullptr) {
                      window->gmic_last_draft_bounds_ = main->bounds;
                    }
                  }
                }
                if (const auto relaunch = weak_launch.lock(); relaunch != nullptr) {
                  (*relaunch)(Stage::Full, next, generation, render->ok);
                  return;
                }
              }
            } else if (current && render->ok) {
              shown->render = render;
              shown->choice = choice;
              if (dialog_guard != nullptr) {
                // What applying will do that a glance at the canvas may miss (layers
                // added, a layer resized) is spelled out under the filter's name.
                dialog_guard->set_preview_message(QString());
                dialog_guard->set_result_notes(
                    gmic_result_notes(*render, active_id, document_width, document_height));
                if (untouched) {
                  // A filter may return new values for its own controls, and a pressed
                  // button has been used; what is shown is the result for those settings.
                  dialog_guard->apply_status(choice.filter_id, render->status);
                  dialog_guard->release_buttons();
                  if (const auto updated = dialog_guard->choice();
                      updated.has_value() && updated->filter_id == choice.filter_id) {
                    shown->choice = updated;
                  }
                }
              }
              shown->preview = render;
              shown->preview_choice = shown->choice;
              if (shown->enabled) {
                put_on_layer(*render);
              }
            } else if (current && !render->aborted) {
              show_original();
              if (dialog_guard != nullptr) {
                if (untouched) {
                  dialog_guard->release_buttons();
                }
                dialog_guard->set_result_notes({});
                dialog_guard->set_preview_message(
                    gmic_run_was_refused(*render)
                        ? gmic_failure_text(*render, name)
                        : MainWindow::tr("Filter preview failed: %1").arg(gmic_failure_text(*render, name)));
              }
            }
            if (!preview_state->closed && preview_state->pending.has_value() && preview_state->start &&
                shown->live_runs < kMaxLivePreviewRuns) {
              auto next = *preview_state->pending;
              preview_state->pending.reset();
              preview_state->start(next);
            }
          },
          Qt::QueuedConnection);
    });
  };

  // Starts the run for a request with a filter, or clears the preview for one without.
  preview_state->start = [this, preview_state, shown, show_original, run_input, dialog_guard, launch,
                          &catalog](const GmicPreviewRequest& request) {
    const auto* filter = request.choice.has_value() ? catalog.find(request.choice->filter_id) : nullptr;
    if (filter == nullptr) {
      preview_state->pending.reset();
      ++preview_state->generation;
      show_original();
      if (dialog_guard != nullptr) {
        dialog_guard->set_preview_message(QString());
        dialog_guard->set_result_notes({});
      }
      return;
    }
    const auto generation = ++preview_state->generation;
    // A draft is worth its extra run when the image is large, the filter may change one of
    // the layers (a draft shows nothing else), and the full run is not known to be quick.
    const auto input = run_input(*filter, *request.choice);
    bool draft = gmic_draft_scale(input) < 1.0 && gmic_has_writable_input(input);
    if (const auto measured = gmic_full_ms_per_megapixel_.find(request.choice->filter_id);
        draft && measured != gmic_full_ms_per_megapixel_.end()) {
      draft = measured->second * gmic_input_megapixels(input) >= 400.0;
    }
    (*launch)(draft ? Stage::Draft : Stage::Full, *request.choice, generation, false);
  };

  dialog.set_preview_callback([preview_state, shown, put_on_layer, show_original_pixels](
                                  const GmicPreviewRequest& request) {
    if (preview_state->closed || !preview_state->start) {
      return;
    }
    const auto running_is = [&shown](const GmicFilterChoice& choice) {
      return shown->running != nullptr && !shown->running->abort.load() &&
             same_choice(shown->running_choice, choice);
    };
    if (!request.choice.has_value()) {
      if (shown->running != nullptr) {
        shown->running->abort.store(true);
      }
      preview_state->start(request);
      return;
    }
    const auto& choice = *request.choice;
    shown->enabled = request.enabled;
    if (!request.enabled) {
      // Preview off only hides the result: the run carries on and what it produces is
      // kept, so turning Preview on shows it at once. A run for other settings stops.
      show_original_pixels();
      if (shown->running != nullptr && !running_is(choice)) {
        shown->running->abort.store(true);
      }
      return;
    }
    if (shown->render != nullptr && same_choice(shown->choice, choice)) {
      // The full result for these settings is already here. A run for other settings is
      // stale: stop it, and make sure its result can no longer replace this one.
      if (shown->running != nullptr && !running_is(choice)) {
        shown->running->abort.store(true);
        ++preview_state->generation;
      }
      preview_state->pending.reset();
      put_on_layer(*shown->render);
      return;
    }
    if (running_is(choice)) {
      // Already rendering these settings: show what has arrived for them and let it finish.
      if (shown->preview != nullptr && same_choice(shown->preview_choice, choice)) {
        put_on_layer(*shown->preview);
      }
      preview_state->pending.reset();
      return;
    }
    // New settings: the run in flight is stale. Ask it to stop and start the new one now
    // instead of waiting for the old one to notice.
    if (shown->running != nullptr) {
      shown->running->abort.store(true);
    }
    if (shown->live_runs >= kMaxLivePreviewRuns) {
      preview_state->pending = request;
      return;
    }
    preview_state->pending.reset();
    preview_state->start(request);
  });

  // Point parameters become draggable markers on the canvas. A position is a percentage
  // of the image the filter works on: the active layer (each chosen layer gets the same
  // percentages of its own), or the document when the layers are lined up.
  const QPointer<CanvasWidget> handle_canvas(canvas_);
  dialog.set_points_callback([this, handle_canvas, dialog_guard, bounds, run_input,
                              &catalog](const std::vector<QPointF>& percent) {
    if (handle_canvas == nullptr) {
      return;
    }
    const auto choice = dialog_guard != nullptr ? dialog_guard->choice() : std::nullopt;
    if (percent.empty() || !choice.has_value()) {
      handle_canvas->clear_preview_point_handles();
      return;
    }
    const auto* filter = catalog.find(choice->filter_id);
    const bool lined_up = filter != nullptr && run_input(*filter, *choice).document_space;
    const QRectF frame = lined_up
                             ? QRectF(0, 0, std::as_const(document()).width(), std::as_const(document()).height())
                             : QRectF(bounds.x, bounds.y, bounds.width, bounds.height);
    std::vector<QPointF> positions;
    positions.reserve(percent.size());
    for (const auto& point : percent) {
      positions.emplace_back(frame.x() + point.x() / 100.0 * frame.width(),
                             frame.y() + point.y() / 100.0 * frame.height());
    }
    handle_canvas->set_preview_point_handles(std::move(positions),
                                             [dialog_guard, frame](int index, QPointF position, bool) {
                                               if (dialog_guard == nullptr || frame.width() <= 0.0 ||
                                                   frame.height() <= 0.0) {
                                                 return;
                                               }
                                               dialog_guard->move_point(
                                                   index, QPointF((position.x() - frame.x()) / frame.width() * 100.0,
                                                                  (position.y() - frame.y()) / frame.height() * 100.0));
                                             });
  });
  const auto remove_point_handles = qScopeGuard([handle_canvas, dialog_guard] {
    if (dialog_guard != nullptr) {
      dialog_guard->set_points_callback({});
    }
    if (handle_canvas != nullptr) {
      handle_canvas->clear_preview_point_handles();
    }
  });

  // The canvas badge carries the running preview's percentage, for the filters that report one.
  QTimer progress_timer(&dialog);
  progress_timer.setInterval(150);
  connect(&progress_timer, &QTimer::timeout, &dialog, [handle_canvas, shown] {
    if (handle_canvas != nullptr) {
      handle_canvas->set_preview_render_progress(
          shown->running != nullptr ? static_cast<int>(std::lround(shown->running->progress.load())) : -1);
    }
  });
  progress_timer.start();

  // The Layers panel's eyes stay usable while the dialog is open, so the layers a filter
  // adds (or the ones under them) can be hidden to inspect the result; nothing else there.
  preview_lock_allows_layer_visibility_ = true;
  const auto end_layer_visibility_lock = qScopeGuard([this] {
    preview_lock_allows_layer_visibility_ = false;
    update_document_action_state();
  });
  auto preview_edit_lock = lock_preview_dialog_edits();
  auto preview_cleanup = qScopeGuard([this, &doc, preview_state, shown, show_original_pixels,
                                      original = *source_layer] {
    close_async_pixel_preview(preview_state);
    if (shown->running != nullptr) {
      shown->running->abort.store(true);
    }
    show_original_pixels();
    if (auto* target = doc.find_layer(original.id()); target != nullptr) {
      *target = original;
      canvas_->document_changed();
    }
  });

  if (!last_gmic_filter_id_.empty()) {
    dialog.select_filter(last_gmic_filter_id_, last_gmic_values_, last_gmic_input_mode_);
  }
  // Non-modal like the other filter dialogs, so the canvas can be zoomed and
  // panned to inspect the preview; edits stay locked (lock_preview_dialog_edits).
  const bool accepted = run_non_modal_dialog(dialog) == QDialog::Accepted;
  const auto choice = dialog.choice();

  dialog.set_points_callback({});
  if (handle_canvas != nullptr) {
    handle_canvas->clear_preview_point_handles();
  }
  close_async_pixel_preview(preview_state);
  // The preview result for exactly the accepted settings is the result to commit.
  std::shared_ptr<GmicRenderResult> ready;
  if (accepted && choice.has_value() && same_choice(shown->choice, *choice)) {
    ready = shown->render;
  }
  // Failing that, a full-quality run already under way for those settings is waited for.
  std::shared_ptr<gmic::RunControl> adopted_control;
  std::shared_future<std::shared_ptr<GmicRenderResult>> adopted_result;
  if (accepted && ready == nullptr && choice.has_value() && same_choice(shown->full_choice, *choice) &&
      shown->full_control != nullptr && !shown->full_control->abort.load() && shown->full_result.valid()) {
    adopted_control = shown->full_control;
    adopted_result = shown->full_result;
  }
  if (shown->running != nullptr && shown->running != adopted_control) {
    shown->running->abort.store(true);
  }
  // The eyes the previewed layers had, for the layers OK adds in their place.
  std::vector<bool> added_eyes;
  for (const auto id : shown->added) {
    const auto* layer = std::as_const(doc).find_layer(id);
    added_eyes.push_back(layer == nullptr || layer->visible());
  }
  const auto keep_added_eyes = [&added_eyes](GmicRenderResult& render) {
    if (render.new_layers.size() == added_eyes.size()) {
      for (std::size_t index = 0; index < added_eyes.size(); ++index) {
        render.new_layers[index].visible = added_eyes[index];
      }
    }
  };
  show_original();
  preview_cleanup.dismiss();
  preview_edit_lock.release();

  const auto* filter = choice.has_value() ? catalog.find(choice->filter_id) : nullptr;
  if (!accepted || filter == nullptr) {
    statusBar()->showMessage(tr("Cancelled %1").arg(library_name));
    return;
  }
  // Remembered even when the run fails or is cancelled, so the dialog reopens there.
  last_gmic_filter_id_ = choice->filter_id;
  last_gmic_values_ = choice->values;
  gmic::release_button_values(*filter, last_gmic_values_);
  last_gmic_input_mode_ = choice->input_mode;
  const QString name = gmic_text(filter->name);
  if (ready != nullptr) {
    keep_added_eyes(*ready);
    commit_gmic_render(*active, name, *ready);
    return;
  }
  if (adopted_control != nullptr) {
    wait_for_gmic_run(this, canvas_, name, *adopted_control, [&adopted_result] {
      return adopted_result.wait_for(std::chrono::milliseconds(15)) == std::future_status::ready;
    });
    // The worker's own completion is ignored now (the preview is closed), so this result
    // has no other user.
    keep_added_eyes(*adopted_result.get());
    commit_gmic_render(*active, name, *adopted_result.get());
    return;
  }
  auto render = render_gmic_with_progress(name, run_input(*filter, *choice));
  keep_added_eyes(render);
  commit_gmic_render(*active, name, render);
}

}  // namespace patchy::ui
