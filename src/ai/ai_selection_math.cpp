#include "ai/ai_selection_math.hpp"

#include "core/resample.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace patchy::ai {
namespace {

// The ImageNet statistics BiRefNet was trained with, on 0..1 RGB.
constexpr float kImageNetMean[3] = {0.485F, 0.456F, 0.406F};
constexpr float kImageNetStd[3] = {0.229F, 0.224F, 0.225F};

// Coverage rounds to zero below these logits, so samples under them cannot light any
// document pixel (bilinear interpolation never exceeds its largest corner).
// Coverage per logit unit around SAM's 0 boundary: full transition within +-0.5 logit, about
// a pixel at real edges. A shallower 64 left wide areas SAM was merely unsure about (logits -2
// to 0) partly selected, which showed as dotted marching-ant holes and background slivers.
constexpr float kSamEdgeRampSlope = 256.0F;
constexpr float kSamVisibleLogit = -127.0F / kSamEdgeRampSlope;
constexpr float kSigmoidVisibleLogit = -6.25F;

void require_rgba8(const PixelBuffer& pixels) {
  if (pixels.format() != PixelFormat::rgba8() || pixels.empty()) {
    throw std::invalid_argument("automatic selection needs a non-empty RGBA8 image");
  }
}

// Resizes RGBA8 with premultiplied interpolation (area-averaging on reductions).
PixelBuffer resized_rgba(const PixelBuffer& rgba8, std::int32_t width, std::int32_t height) {
  if (width == rgba8.width() && height == rgba8.height()) {
    return rgba8;
  }
  return resample_pixels(rgba8, width, height, ResampleMethod::Bilinear);
}

// Composites one channel over white, the backdrop a flattened document shows.
float over_white(std::uint8_t color, std::uint8_t alpha) noexcept {
  const auto a = static_cast<float>(alpha) / 255.0F;
  return static_cast<float>(color) * a + 255.0F * (1.0F - a);
}

float visible_logit(LogitMapping mapping) noexcept {
  return mapping == LogitMapping::SamEdgeRamp ? kSamVisibleLogit : kSigmoidVisibleLogit;
}

}  // namespace

SamTransform sam_transform(std::int32_t source_width, std::int32_t source_height) noexcept {
  SamTransform transform;
  transform.source_width = std::max(1, source_width);
  transform.source_height = std::max(1, source_height);
  const auto longest = std::max(transform.source_width, transform.source_height);
  transform.scale = static_cast<double>(kSamInputSize) / static_cast<double>(longest);
  transform.scaled_width = std::clamp(
      static_cast<std::int32_t>(std::lround(transform.source_width * transform.scale)), 1, kSamInputSize);
  transform.scaled_height = std::clamp(
      static_cast<std::int32_t>(std::lround(transform.source_height * transform.scale)), 1, kSamInputSize);
  return transform;
}

SamPoint to_sam_point(const SamTransform& transform, double x, double y) noexcept {
  return SamPoint{static_cast<float>(x * transform.scale), static_cast<float>(y * transform.scale)};
}

std::vector<float> sam_encoder_input(const PixelBuffer& rgba8, const SamTransform& transform) {
  require_rgba8(rgba8);
  const auto resized = resized_rgba(rgba8, transform.scaled_width, transform.scaled_height);
  std::vector<float> input(static_cast<std::size_t>(transform.scaled_width) *
                           static_cast<std::size_t>(transform.scaled_height) * 3U);
  auto* out = input.data();
  for (std::int32_t y = 0; y < resized.height(); ++y) {
    const auto row = resized.row(y);
    for (std::int32_t x = 0; x < resized.width(); ++x) {
      const auto* pixel = row.data() + static_cast<std::size_t>(x) * 4U;
      *out++ = over_white(pixel[0], pixel[3]);
      *out++ = over_white(pixel[1], pixel[3]);
      *out++ = over_white(pixel[2], pixel[3]);
    }
  }
  return input;
}

std::vector<float> subject_model_input(const PixelBuffer& rgba8) {
  require_rgba8(rgba8);
  const auto resized = resized_rgba(rgba8, kSubjectInputSize, kSubjectInputSize);
  const auto plane = static_cast<std::size_t>(kSubjectInputSize) * static_cast<std::size_t>(kSubjectInputSize);
  std::vector<float> input(plane * 3U);
  for (std::int32_t y = 0; y < kSubjectInputSize; ++y) {
    const auto row = resized.row(y);
    const auto offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(kSubjectInputSize);
    for (std::int32_t x = 0; x < kSubjectInputSize; ++x) {
      const auto* pixel = row.data() + static_cast<std::size_t>(x) * 4U;
      for (std::size_t channel = 0; channel < 3U; ++channel) {
        const auto value = over_white(pixel[channel], pixel[3]) / 255.0F;
        input[channel * plane + offset + static_cast<std::size_t>(x)] =
            (value - kImageNetMean[channel]) / kImageNetStd[channel];
      }
    }
  }
  return input;
}

std::uint8_t logit_to_coverage(float logit, LogitMapping mapping) noexcept {
  float value = 0.0F;
  if (mapping == LogitMapping::SamEdgeRamp) {
    value = 127.5F + kSamEdgeRampSlope * logit;
  } else {
    value = 255.0F / (1.0F + std::exp(-logit));
  }
  return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

AiMask document_mask_from_logits(const float* logits, std::int32_t model_width, std::int32_t model_height,
                                 std::int32_t document_width, std::int32_t document_height,
                                 LogitMapping mapping) {
  AiMask mask;
  if (logits == nullptr || model_width <= 0 || model_height <= 0 || document_width <= 0 ||
      document_height <= 0) {
    return mask;
  }
  const auto threshold = visible_logit(mapping);
  std::int32_t min_mx = model_width;
  std::int32_t min_my = model_height;
  std::int32_t max_mx = -1;
  std::int32_t max_my = -1;
  for (std::int32_t my = 0; my < model_height; ++my) {
    const auto* row = logits + static_cast<std::size_t>(my) * static_cast<std::size_t>(model_width);
    for (std::int32_t mx = 0; mx < model_width; ++mx) {
      if (row[mx] > threshold) {
        min_mx = std::min(min_mx, mx);
        max_mx = std::max(max_mx, mx);
        min_my = std::min(min_my, my);
        max_my = std::max(max_my, my);
      }
    }
  }
  if (max_mx < 0) {
    return mask;
  }

  // Document pixel x samples the grid at (x + 0.5) * model / document - 0.5; only pixels
  // whose sample lands within one cell of a visible sample can be covered.
  const double scale_x = static_cast<double>(model_width) / static_cast<double>(document_width);
  const double scale_y = static_cast<double>(model_height) / static_cast<double>(document_height);
  const auto document_span = [](std::int32_t model_min, std::int32_t model_max, double scale,
                                std::int32_t document_size) {
    const auto first = static_cast<std::int32_t>(std::floor((model_min - 1 + 0.5) / scale - 0.5));
    const auto last = static_cast<std::int32_t>(std::ceil((model_max + 1 + 0.5) / scale - 0.5));
    return std::pair{std::clamp(first, 0, document_size - 1), std::clamp(last, 0, document_size - 1)};
  };
  const auto [x0, x1] = document_span(min_mx, max_mx, scale_x, document_width);
  const auto [y0, y1] = document_span(min_my, max_my, scale_y, document_height);
  const auto span_width = x1 - x0 + 1;
  const auto span_height = y1 - y0 + 1;

  // Per-column sample positions are shared by every row.
  std::vector<std::int32_t> column_low(static_cast<std::size_t>(span_width));
  std::vector<float> column_fraction(static_cast<std::size_t>(span_width));
  for (std::int32_t i = 0; i < span_width; ++i) {
    const auto sample = std::clamp((x0 + i + 0.5) * scale_x - 0.5, 0.0, static_cast<double>(model_width - 1));
    const auto low = std::min(static_cast<std::int32_t>(sample), model_width - 1);
    column_low[static_cast<std::size_t>(i)] = low;
    column_fraction[static_cast<std::size_t>(i)] = static_cast<float>(sample - low);
  }

  PixelBuffer coverage(span_width, span_height, PixelFormat::gray8());
  std::int32_t min_x = span_width;
  std::int32_t min_y = span_height;
  std::int32_t max_x = -1;
  std::int32_t max_y = -1;
  for (std::int32_t j = 0; j < span_height; ++j) {
    const auto sample = std::clamp((y0 + j + 0.5) * scale_y - 0.5, 0.0, static_cast<double>(model_height - 1));
    const auto low = std::min(static_cast<std::int32_t>(sample), model_height - 1);
    const auto high = std::min(low + 1, model_height - 1);
    const auto fy = static_cast<float>(sample - low);
    const auto* top = logits + static_cast<std::size_t>(low) * static_cast<std::size_t>(model_width);
    const auto* bottom = logits + static_cast<std::size_t>(high) * static_cast<std::size_t>(model_width);
    auto out = coverage.row(j);
    for (std::int32_t i = 0; i < span_width; ++i) {
      const auto lx = column_low[static_cast<std::size_t>(i)];
      const auto hx = std::min(lx + 1, model_width - 1);
      const auto fx = column_fraction[static_cast<std::size_t>(i)];
      const auto upper = top[lx] + (top[hx] - top[lx]) * fx;
      const auto lower = bottom[lx] + (bottom[hx] - bottom[lx]) * fx;
      const auto value = logit_to_coverage(upper + (lower - upper) * fy, mapping);
      out[static_cast<std::size_t>(i)] = value;
      if (value != 0U) {
        min_x = std::min(min_x, i);
        max_x = std::max(max_x, i);
        min_y = std::min(min_y, j);
        max_y = std::max(max_y, j);
      }
    }
  }
  if (max_x < 0) {
    return mask;
  }

  mask.bounds = Rect{x0 + min_x, y0 + min_y, max_x - min_x + 1, max_y - min_y + 1};
  if (mask.bounds.width == span_width && mask.bounds.height == span_height) {
    mask.alpha = std::move(coverage);
    return mask;
  }
  mask.alpha = PixelBuffer(mask.bounds.width, mask.bounds.height, PixelFormat::gray8());
  for (std::int32_t j = 0; j < mask.bounds.height; ++j) {
    const auto source = std::as_const(coverage).row(min_y + j);
    std::memcpy(mask.alpha.row(j).data(), source.data() + min_x, static_cast<std::size_t>(mask.bounds.width));
  }
  return mask;
}

AiMask clip_mask_to_rect(const AiMask& mask, Rect rect) {
  if (mask.empty()) {
    return mask;
  }
  const auto left = std::max(mask.bounds.x, rect.x);
  const auto top = std::max(mask.bounds.y, rect.y);
  const auto right = std::min(mask.bounds.x + mask.bounds.width, rect.x + rect.width);
  const auto bottom = std::min(mask.bounds.y + mask.bounds.height, rect.y + rect.height);
  AiMask clipped;
  if (left >= right || top >= bottom) {
    return clipped;
  }
  std::int32_t min_x = right;
  std::int32_t min_y = bottom;
  std::int32_t max_x = -1;
  std::int32_t max_y = -1;
  for (auto y = top; y < bottom; ++y) {
    const auto row = mask.alpha.row(y - mask.bounds.y);
    for (auto x = left; x < right; ++x) {
      if (row[static_cast<std::size_t>(x - mask.bounds.x)] != 0U) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
      }
    }
  }
  if (max_x < 0) {
    return clipped;
  }
  clipped.bounds = Rect{min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
  clipped.alpha = PixelBuffer(clipped.bounds.width, clipped.bounds.height, PixelFormat::gray8());
  for (std::int32_t y = 0; y < clipped.bounds.height; ++y) {
    const auto source = mask.alpha.row(clipped.bounds.y + y - mask.bounds.y);
    std::memcpy(clipped.alpha.row(y).data(), source.data() + (clipped.bounds.x - mask.bounds.x),
                static_cast<std::size_t>(clipped.bounds.width));
  }
  return clipped;
}

double mask_iou(const AiMask& a, const AiMask& b) noexcept {
  if (a.empty() && b.empty()) {
    return 1.0;
  }
  const auto covered = [](const AiMask& mask, std::int32_t x, std::int32_t y) {
    if (mask.empty() || !mask.bounds.contains(x, y)) {
      return false;
    }
    return mask.alpha.pixel(x - mask.bounds.x, y - mask.bounds.y)[0] >= 128U;
  };
  const auto& first = a.empty() ? b : a;
  auto left = first.bounds.x;
  auto top = first.bounds.y;
  auto right = first.bounds.x + first.bounds.width;
  auto bottom = first.bounds.y + first.bounds.height;
  if (!a.empty() && !b.empty()) {
    left = std::min(a.bounds.x, b.bounds.x);
    top = std::min(a.bounds.y, b.bounds.y);
    right = std::max(a.bounds.x + a.bounds.width, b.bounds.x + b.bounds.width);
    bottom = std::max(a.bounds.y + a.bounds.height, b.bounds.y + b.bounds.height);
  }
  std::size_t intersection = 0;
  std::size_t union_count = 0;
  for (auto y = top; y < bottom; ++y) {
    for (auto x = left; x < right; ++x) {
      const bool in_a = covered(a, x, y);
      const bool in_b = covered(b, x, y);
      intersection += (in_a && in_b) ? 1U : 0U;
      union_count += (in_a || in_b) ? 1U : 0U;
    }
  }
  return union_count == 0U ? 1.0 : static_cast<double>(intersection) / static_cast<double>(union_count);
}

}  // namespace patchy::ai
