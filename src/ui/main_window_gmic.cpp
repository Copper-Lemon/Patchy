// MainWindow's G'MIC Filter Library (docs/gmic.md): the command that opens the
// library dialog with a live preview on the active layer, and running a chosen
// filter through the engine, limited to the selection and undoable.
//
// Every engine run happens off the UI thread, pixel conversion included. The
// dialog's preview follows the shared pattern of the filter dialogs
// (AsyncPixelPreviewState, the canvas "Rendering preview..." badge), and also
// asks a run that newer settings have made stale to stop.
//
// Layers: a filter reads the layers its input mode names and only ever changes
// the active one. Its first output replaces the active layer; every further
// output becomes a new layer above it.

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
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace patchy::ui {

// One layer a filter reads.
struct GmicInputLayer {
  std::shared_ptr<const PixelBuffer> pixels;
  Rect bounds{};
  std::string name;
};

// What one run needs, all immutable or copied so a worker can hold it.
struct GmicRunInput {
  // The active layer as it was before any preview touched it.
  std::shared_ptr<const PixelBuffer> original;
  Rect bounds{};
  std::string layer_name;
  // False: the filter gets the active layer alone, at its own size. True: it gets
  // `layers` (top of the stack first), each placed on a document-sized image so
  // they line up.
  bool document_space{false};
  // Below 1: a draft. The filter runs on images reduced by this factor and the active
  // layer's result is enlarged back to the full frame; layers a filter adds are left out.
  // Sizes a filter takes in pixels (a blur radius) are not rescaled, so a draft only
  // approximates the full run.
  double scale{1.0};
  std::vector<GmicInputLayer> layers;
  // Where the active layer sits in `layers`; none when the input mode leaves it out
  // (a visible active layer with "all hidden layers").
  std::optional<std::size_t> active_input;
  std::int32_t document_width{0};
  std::int32_t document_height{0};
  FilterSelection selection;
  std::string command;
  std::string filter_name;      // names an output layer the filter left unnamed
  PixelFormat new_layer_format; // RGBA at the document's bit depth
};

// A further output of a filter, to be inserted as a new layer.
struct GmicNewLayer {
  PixelBuffer pixels;
  Rect bounds{};
  std::string name;
  BlendMode blend_mode{BlendMode::Normal};
  float opacity{1.0F};
};

// One filter run turned back into layer pixels. Built on a worker thread.
struct GmicRenderResult {
  bool ok{false};
  bool aborted{false};
  // The filter resized its image while a selection exists; nothing was produced.
  bool size_refused{false};
  // The input mode named no layer; nothing ran.
  bool no_input{false};
  // The run would need more memory than the computer can spare; nothing ran.
  std::int64_t memory_refused_mb{0};
  bool changed{true};  // false when the active layer's result equals the original
  std::string error;   // the interpreter's message when it failed
  std::string status;  // what the filter returned for its controls
  PixelBuffer pixels;  // the active layer's new pixels
  Rect bounds{};
  std::vector<GmicNewLayer> new_layers;  // bottom of the new stack first
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

// The float images a run holds at its start, in MB: Patchy's planar copy of every layer
// it reads, the interpreter's own copy, and as much again for a result.
[[nodiscard]] std::int64_t gmic_run_memory_mb(const GmicRunInput& input) {
  std::int64_t samples = 0;
  if (input.document_space) {
    samples = static_cast<std::int64_t>(input.document_width) * input.document_height * 4 *
              static_cast<std::int64_t>(input.layers.size());
  } else {
    samples = static_cast<std::int64_t>(input.original->width()) * input.original->height() *
              (input.original->format().channels >= 4 ? 4 : 3);
  }
  const double scale = std::clamp(input.scale, 0.01, 1.0);
  return static_cast<std::int64_t>(static_cast<double>(samples) * scale * scale) *
         static_cast<std::int64_t>(sizeof(float)) * 3 / (1024 * 1024);
}

// The factor a draft of this run is reduced by: about a megapixel of input (or
// PATCHY_GMIC_DRAFT_PIXELS, for tests). 1 when the input is already near that size.
[[nodiscard]] double gmic_draft_scale(const GmicRunInput& input) {
  double budget = 1'000'000.0;
  if (const auto pixels = environment_variable("PATCHY_GMIC_DRAFT_PIXELS"); pixels.has_value()) {
    if (const auto value = std::atof(pixels->c_str()); value >= 1.0) {
      budget = value;
    }
  }
  const double pixels = input.document_space
                            ? static_cast<double>(input.document_width) * static_cast<double>(input.document_height)
                            : static_cast<double>(input.original->width()) *
                                  static_cast<double>(input.original->height());
  return pixels <= budget * 1.5 ? 1.0 : std::sqrt(budget / pixels);
}

[[nodiscard]] double gmic_input_megapixels(const GmicRunInput& input) {
  const double pixels = input.document_space
                            ? static_cast<double>(input.document_width) * static_cast<double>(input.document_height) *
                                  static_cast<double>(std::max<std::size_t>(1U, input.layers.size()))
                            : static_cast<double>(input.original->width()) *
                                  static_cast<double>(input.original->height());
  return pixels / 1'000'000.0;
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

[[nodiscard]] GmicRenderResult render_gmic_filter_unguarded(const GmicRunInput& input, gmic::RunControl* control);

// The whole run: layers to float, the engine, float back to layer formats, and
// the selection. Thread-safe; touches no widget and translates nothing. Never
// throws: it runs on worker threads, where an escaped exception (a float copy
// of a large document can exhaust memory) would end the process.
[[nodiscard]] GmicRenderResult render_gmic_filter(const GmicRunInput& input, gmic::RunControl* control) {
  try {
    return render_gmic_filter_unguarded(input, control);
  } catch (const std::exception& error) {
    GmicRenderResult failed;
    failed.bounds = input.bounds;
    failed.error = error.what();
    return failed;
  } catch (...) {
    GmicRenderResult failed;
    failed.bounds = input.bounds;
    failed.error = "Unknown G'MIC failure.";
    return failed;
  }
}

[[nodiscard]] GmicRenderResult render_gmic_filter_unguarded(const GmicRunInput& input, gmic::RunControl* control) {
  GmicRenderResult render;
  render.bounds = input.bounds;
  const PixelBuffer& original = *input.original;
  if (input.document_space && input.layers.empty()) {
    render.no_input = true;
    return render;
  }
  if (const auto needed_mb = gmic_run_memory_mb(input); needed_mb > gmic_memory_budget_mb()) {
    render.memory_refused_mb = needed_mb;
    return render;
  }
  // The image the filter worked on: the document, or the active layer alone.
  const Rect frame = input.document_space ? Rect{0, 0, input.document_width, input.document_height} : input.bounds;
  const double scale = std::clamp(input.scale, 0.01, 1.0);
  const bool draft = scale < 1.0;
  const int run_width = draft ? std::max(1, static_cast<int>(std::lround(frame.width * scale))) : frame.width;
  const int run_height = draft ? std::max(1, static_cast<int>(std::lround(frame.height * scale))) : frame.height;
  std::vector<gmic::Image> images;
  const auto add_image = [&](gmic::Image image, const std::string& name) {
    images.push_back(draft ? reduced_image(image, run_width, run_height) : std::move(image));
    // Hosts name their layers; several filters read the name.
    images.back().name = name.empty() ? std::string("Layer") : name;
  };
  if (input.document_space) {
    images.reserve(input.layers.size());
    for (const auto& layer : input.layers) {
      add_image(gmic_document_image(*layer.pixels, layer.bounds, input.document_width, input.document_height),
                layer.name);
    }
  } else {
    add_image(gmic_image_from_pixels(original), input.layer_name);
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
  // Which outputs go where. A filter that returns at least one image per layer it read
  // worked on each of them: the image in the active layer's place is its result, and the
  // images in the places of the layers that were only read are dropped (those layers are
  // never changed, and adding them again would duplicate them). A filter that returns
  // fewer images merged its input: the first image is the result. With no active layer
  // among the input, every image is a new layer and the active layer keeps its pixels.
  const std::size_t input_count = input.document_space ? input.layers.size() : 1U;
  const bool per_layer = images.size() >= input_count;
  std::optional<std::size_t> active_output;
  if (input.active_input.has_value()) {
    active_output = per_layer ? *input.active_input : 0U;
  }
  const auto is_new_layer = [&](std::size_t index) {
    if (!active_output.has_value()) {
      return true;
    }
    return index != *active_output && !(per_layer && index < input_count);
  };
  if (active_output.has_value() &&
      (images[*active_output].width <= 0 || images[*active_output].height <= 0)) {
    return render;
  }

  const auto placed = [&frame, scale](const gmic::Image& image, const gmic::OutputLayerInfo& info) {
    Rect bounds{frame.x, frame.y, image.width, image.height};
    if (info.position.has_value()) {
      // A position is in the pixels of the image the filter ran on.
      bounds.x += static_cast<int>(std::lround(info.position->first / scale));
      bounds.y += static_cast<int>(std::lround(info.position->second / scale));
    } else if (image.width != frame.width || image.height != frame.height) {
      // A resized result with no stated position stays centred on what it replaces.
      bounds.x += (frame.width - image.width) / 2;
      bounds.y += (frame.height - image.height) / 2;
    }
    return bounds;
  };

  bool same_frame = false;
  if (active_output.has_value()) {
    if (draft) {
      // Back to the full frame: exactly, when the filter kept its image's size.
      auto& reduced = images[*active_output];
      const bool kept_size = reduced.width == run_width && reduced.height == run_height;
      const int width = kept_size ? frame.width : std::max(1, static_cast<int>(std::lround(reduced.width / scale)));
      const int height =
          kept_size ? frame.height : std::max(1, static_cast<int>(std::lround(reduced.height / scale)));
      reduced = enlarged_image(reduced, width, height);
    }
    const auto& result_image = images[*active_output];
    render.pixels = pixels_from_gmic_image(result_image, original.format());
    render.bounds = placed(result_image, gmic::parse_output_layer_name(result_image.name));
    same_frame = render.bounds.x == frame.x && render.bounds.y == frame.y &&
                 render.pixels.width() == frame.width && render.pixels.height() == frame.height;
    if (!same_frame && !input.selection.isEmpty()) {
      render.size_refused = true;
      return render;
    }
  } else {
    render.pixels = original;  // shared storage, no copy
    render.changed = false;
  }
  if (same_frame) {
    // What the result is compared and blended against, in the same frame.
    const PixelBuffer embedded = input.document_space
                                     ? embed_in_document(original, input.bounds, frame.width, frame.height)
                                     : PixelBuffer();
    const PixelBuffer& before = input.document_space ? embedded : original;
    if (!input.selection.isEmpty()) {
      const auto pixel_bytes = static_cast<std::size_t>(bytes_per_pixel(render.pixels.format()));
      // Restore unselected spans in one region walk, then feather by coverage.
      const auto outside = QRegion(to_qrect(frame)).subtracted(input.selection);
      for (const auto& rect : outside) {
        const auto local = rect.translated(-frame.x, -frame.y);
        for (int y = local.top(); y <= local.bottom(); ++y) {
          std::copy_n(before.pixel(local.left(), y), static_cast<std::size_t>(local.width()) * pixel_bytes,
                      render.pixels.pixel(local.left(), y));
        }
      }
      blend_by_selection_coverage(render.pixels, before, frame, input.selection);
    }
    render.changed = !pixel_buffers_equal(render.pixels, before);
  }
  if (input.document_space && active_output.has_value()) {
    trim_to_content(render.pixels, render.bounds);
  }

  for (std::size_t index = 0; index < images.size(); ++index) {
    const auto& image = images[index];
    // A draft shows the active layer only.
    if (draft || !is_new_layer(index) || image.width <= 0 || image.height <= 0) {
      continue;
    }
    const auto info = gmic::parse_output_layer_name(image.name);
    GmicNewLayer layer;
    layer.pixels = pixels_from_gmic_image(image, input.new_layer_format);
    layer.bounds = placed(image, info);
    if (input.document_space) {
      trim_to_content(layer.pixels, layer.bounds);
    }
    layer.name = info.name.empty() ? input.filter_name : info.name;
    layer.blend_mode = blend_mode_from_gmic(info.blend_mode);
    layer.opacity = info.opacity.has_value() ? static_cast<float>(*info.opacity / 100.0) : 1.0F;
    render.new_layers.push_back(std::move(layer));
  }
  if (!render.new_layers.empty()) {
    render.changed = true;
  }
  render.ok = true;
  return render;
}

struct GmicInputCandidate {
  const Layer* layer{nullptr};
  bool visible{true};  // shown in the document: its own eye and every enclosing group's
};

// Bottom to top, groups opened: every layer that can be shown as RGB pixels.
void collect_input_candidates(const std::vector<Layer>& layers, bool parents_visible,
                              std::vector<GmicInputCandidate>& out) {
  for (const auto& layer : layers) {
    const bool visible = parents_visible && layer.visible();
    if (layer.kind() == LayerKind::Group) {
      collect_input_candidates(layer.children(), visible, out);
    } else if (layer.kind() != LayerKind::Adjustment && layer.pixels().width() > 0 && layer.pixels().height() > 0 &&
               layer.pixels().format().color_mode == ColorMode::RGB && layer.pixels().format().channels >= 3) {
      out.push_back({&layer, visible});
    }
  }
}

// Fills the layers a run reads, top of the stack first, and where the active layer is
// among them. The active layer is taken as `input.original` (the document may be showing
// a preview in its place).
void set_gmic_input_layers(GmicRunInput& input, const Document& document, LayerId active, char mode) {
  const auto& original = input.original;
  const auto bounds = input.bounds;
  const auto& layer_name = input.layer_name;
  std::vector<GmicInputCandidate> candidates;
  collect_input_candidates(document.layers(), true, candidates);
  std::reverse(candidates.begin(), candidates.end());
  const auto active_at =
      std::find_if(candidates.begin(), candidates.end(),
                   [active](const GmicInputCandidate& candidate) { return candidate.layer->id() == active; });
  auto& inputs = input.layers;
  inputs.clear();
  input.active_input.reset();
  const auto add = [&](const Layer* layer) {
    if (layer->id() == active) {
      input.active_input = inputs.size();
      inputs.push_back({original, bounds, layer_name});
    } else {
      inputs.push_back({std::make_shared<const PixelBuffer>(layer->pixels()), layer->bounds(), layer->name()});
    }
  };
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    const bool is_active = it == active_at;
    bool wanted = false;
    switch (mode) {
      case '*': wanted = true; break;
      case 'v': wanted = it->visible; break;
      case 'i': wanted = !it->visible; break;
      case '+': wanted = is_active || (active_at != candidates.end() && it == active_at + 1); break;
      case '-': wanted = is_active || (active_at != candidates.end() && it + 1 == active_at); break;
      default: wanted = is_active; break;
    }
    if (wanted) {
      add(it->layer);
    }
  }
}

// Puts a finished run into the document: the active layer's new pixels, then every
// further output as a new layer stacked above it in the order the filter returned
// them. Returns how many layers were added. The caller owns undo and refresh.
std::size_t write_gmic_render(Document& document, LayerId layer_id, GmicRenderResult& render) {
  auto* layer = document.find_layer(layer_id);
  if (layer == nullptr) {
    return 0;
  }
  const auto bounds = render.bounds;
  set_layer_pixels_with_bounds(*layer, std::move(render.pixels), bounds);
  std::optional<LayerId> anchor = layer_id;
  for (auto& output : render.new_layers) {
    Layer added_layer(document.allocate_layer_id(), std::move(output.name), std::move(output.pixels));
    added_layer.set_bounds(output.bounds);
    added_layer.set_blend_mode(output.blend_mode);
    added_layer.set_opacity(output.opacity);
    const auto added_id = added_layer.id();
    insert_layer_after_anchor(document, std::move(added_layer), anchor);
    anchor = added_id;
  }
  return render.new_layers.size();
}

// Everything a run reads, gathered on the UI thread (shared buffers, no pixel copies).
[[nodiscard]] GmicRunInput make_gmic_run_input(const Document& document, const Layer& layer,
                                               const std::shared_ptr<const PixelBuffer>& original,
                                               const gmic::Filter& filter, const std::vector<std::string>& values,
                                               char input_mode, FilterSelection selection) {
  GmicRunInput input;
  input.original = original;
  input.bounds = layer.bounds();
  input.layer_name = layer.name();
  input.document_space = input_mode != '.';
  input.document_width = document.width();
  input.document_height = document.height();
  if (input.document_space) {
    set_gmic_input_layers(input, document, layer.id(), input_mode);
  } else {
    input.active_input = 0U;
  }
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
  return render.size_refused || render.no_input || render.memory_refused_mb > 0;
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

bool MainWindow::commit_gmic_render(LayerId layer_id, const QString& name, GmicRenderResult& render, Rect bounds) {
  if (render.aborted) {
    statusBar()->showMessage(tr("Cancelled %1").arg(name));
    return false;
  }
  if (render.size_refused || render.no_input) {
    show_status_error(gmic_failure_text(render, name));
    return false;
  }
  if (!render.ok) {
    show_critical_message(this, tr("G'MIC Filter Library"),
                          tr("%1 could not be applied.\n\n%2").arg(name, gmic_failure_text(render, name)),
                          QStringLiteral("gmicErrorMessageBox"));
    return false;
  }
  if (!render.changed) {
    statusBar()->showMessage(tr("%1 made no changes").arg(name));
    return true;
  }
  push_undo_snapshot(tr("Filter: %1").arg(name));
  auto& doc = document();
  if (doc.find_layer(layer_id) == nullptr) {
    return false;
  }
  const auto new_bounds = render.bounds;
  const auto added = write_gmic_render(doc, layer_id, render);
  if (added > 0) {
    doc.set_active_layer(layer_id);
    refresh_layer_list();
    refresh_layer_controls();
    canvas_->document_changed();
  } else {
    canvas_->document_changed(to_qrect(bounds).united(to_qrect(new_bounds)));
  }
  schedule_palette_compliance_check();
  statusBar()->showMessage(added > 0 ? tr("Applied %1 (new layers: %2)").arg(name).arg(static_cast<qulonglong>(added))
                                     : tr("Applied %1").arg(name));
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
  const auto* layer = doc.find_layer(*active);
  if (layer == nullptr) {
    return false;
  }
  const char mode = input_mode != 0 ? input_mode : filter->input_mode;
  const auto input = make_gmic_run_input(doc, *layer, std::make_shared<const PixelBuffer>(layer->pixels()), *filter,
                                         values, mode, filter_selection(*canvas_));
  const auto bounds = input.bounds;
  auto render = render_gmic_with_progress(name, input);
  return commit_gmic_render(*active, name, render, bounds);
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
  const auto input =
      make_gmic_run_input(doc, *layer, std::make_shared<const PixelBuffer>(layer->pixels()), *filter, values, mode,
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
  if (!render.changed) {
    return GmicApplyStatus::NoChange;
  }
  if (before_write) {
    before_write();
  }
  if (session.document.find_layer(layer_id) == nullptr) {
    return fail(tr("Select an editable RGB pixel layer"));
  }
  const auto added = write_gmic_render(session.document, layer_id, render);
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
  const auto original_pixels = std::make_shared<const PixelBuffer>(source_layer->pixels());
  const auto bounds = source_layer->bounds();
  const auto selection = filter_selection(*canvas_);
  const std::string layer_name = source_layer->name();
  const auto new_layer_format = with_bit_depth(PixelFormat::rgba8(), std::as_const(doc).color_state().bit_depth);

  // Built on the UI thread per request: the layers a choice reads are shared,
  // copy-on-write buffers, so this copies no pixels.
  const auto run_input = [this, active, original_pixels, bounds, selection, layer_name,
                          new_layer_format](const gmic::Filter& filter, const GmicFilterChoice& choice) {
    const auto& current = std::as_const(document());
    GmicRunInput input;
    input.original = original_pixels;
    input.bounds = bounds;
    input.layer_name = layer_name;
    input.document_space = choice.input_mode != '.';
    input.document_width = current.width();
    input.document_height = current.height();
    if (input.document_space) {
      set_gmic_input_layers(input, current, *active, choice.input_mode);
    } else {
      input.active_input = 0U;
    }
    input.selection = selection;
    input.command = gmic::command_line(filter, choice.values, false);
    input.filter_name = filter.name;
    input.new_layer_format = new_layer_format;
    return input;
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
    Rect bounds{};  // the bounds the layer shows now
    // The full-quality run in flight: OK for the same settings waits for it instead of
    // starting over.
    std::optional<GmicFilterChoice> full_choice;
    std::shared_ptr<gmic::RunControl> full_control;
    std::shared_future<std::shared_ptr<GmicRenderResult>> full_result;
    int live_runs{0};     // runs still working, the stale ones that are stopping included
    bool enabled{true};   // the Preview box
  };
  auto shown = std::make_shared<Shown>();
  shown->bounds = bounds;
  auto preview_state = std::make_shared<AsyncPixelPreviewState<GmicPreviewRequest>>();
  // A stale run stops at the interpreter's next check, which can take seconds; the newest
  // settings start at once beside it. Beyond this many runs a new request waits.
  constexpr int kMaxLivePreviewRuns = 2;

  const QPointer<MainWindow> self(this);
  const auto put_on_layer = [self, active, shown](const GmicRenderResult& render) {
    if (self == nullptr) {
      return;
    }
    if (auto* layer = self->document().find_layer(*active); layer != nullptr) {
      set_layer_pixels_with_bounds(*layer, PixelBuffer(render.pixels), render.bounds);
      if (self->canvas_ != nullptr) {
        self->canvas_->document_changed(to_qrect(shown->bounds).united(to_qrect(render.bounds)));
      }
      shown->bounds = render.bounds;
    }
  };
  const auto show_original_pixels = [self, active, original_pixels, bounds, shown] {
    if (self == nullptr) {
      return;
    }
    if (auto* layer = self->document().find_layer(*active); layer != nullptr) {
      set_layer_pixels_preserving_origin(*layer, *original_pixels, bounds);
      if (self->canvas_ != nullptr) {
        self->canvas_->document_changed(to_qrect(shown->bounds).united(to_qrect(bounds)));
      }
    }
    shown->bounds = bounds;
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
    auto* app = QCoreApplication::instance();
    const auto window = QPointer<MainWindow>(this);
    if (canvas_ != nullptr) {
      canvas_->begin_preview_render();
      canvas_->set_preview_render_refining(stage == Stage::Full && after_draft);
    }
    run_tracked_background_worker([app, window, preview_state, shown, put_on_layer, show_original, dialog_guard,
                                   generation, control, choice, name, stage, megapixels, promise, weak_launch,
                                   input = std::move(input)] {
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
           name, stage, megapixels, elapsed_ms, weak_launch, render] {
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
                    window->gmic_last_draft_bounds_ = render->bounds;
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
                // The canvas previews the active layer only; layers a filter adds
                // appear when it is applied.
                dialog_guard->set_preview_message(
                    render->new_layers.empty()
                        ? QString()
                        : MainWindow::tr("New layers on apply: %1")
                              .arg(static_cast<qulonglong>(render->new_layers.size())));
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
      }
      return;
    }
    const auto generation = ++preview_state->generation;
    // A draft is worth its extra run when the image is large, the active layer is among
    // the input (a draft shows nothing else), and the full run is not known to be quick.
    const auto input = run_input(*filter, *request.choice);
    bool draft = gmic_draft_scale(input) < 1.0 && input.active_input.has_value();
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
  // of the image the filter works on: the active layer, or the document when it reads
  // several layers.
  const QPointer<CanvasWidget> handle_canvas(canvas_);
  dialog.set_points_callback([this, handle_canvas, dialog_guard, bounds](const std::vector<QPointF>& percent) {
    if (handle_canvas == nullptr) {
      return;
    }
    const auto choice = dialog_guard != nullptr ? dialog_guard->choice() : std::nullopt;
    if (percent.empty() || !choice.has_value()) {
      handle_canvas->clear_preview_point_handles();
      return;
    }
    const QRectF frame = choice->input_mode == '.'
                             ? QRectF(bounds.x, bounds.y, bounds.width, bounds.height)
                             : QRectF(0, 0, std::as_const(document()).width(), std::as_const(document()).height());
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

  auto preview_edit_lock = lock_preview_dialog_edits();
  auto preview_cleanup = qScopeGuard([this, &doc, preview_state, shown, original = *source_layer] {
    close_async_pixel_preview(preview_state);
    if (shown->running != nullptr) {
      shown->running->abort.store(true);
    }
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
    commit_gmic_render(*active, name, *ready, bounds);
    return;
  }
  if (adopted_control != nullptr) {
    wait_for_gmic_run(this, canvas_, name, *adopted_control, [&adopted_result] {
      return adopted_result.wait_for(std::chrono::milliseconds(15)) == std::future_status::ready;
    });
    // The worker's own completion is ignored now (the preview is closed), so this result
    // has no other user.
    commit_gmic_render(*active, name, *adopted_result.get(), bounds);
    return;
  }
  auto render = render_gmic_with_progress(name, run_input(*filter, *choice));
  commit_gmic_render(*active, name, render, bounds);
}

}  // namespace patchy::ui
