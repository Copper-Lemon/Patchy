#pragma once

// Pre- and post-processing for the automatic-selection models (docs/ai-selection.md).
// Qt-free and independent of ONNX Runtime, so every build compiles and tests it. The
// model outputs these functions consume are CPU-dependent float math, so callers and
// tests compare masks by IoU and never pin bytes.

#include "core/layer.hpp"
#include "core/pixel_buffer.hpp"

#include <cstdint>
#include <vector>

namespace patchy::ai {

// The longest image side the MobileSAM encoder takes; the encoder pads the rest itself.
inline constexpr std::int32_t kSamInputSize = 1024;
// The MobileSAM image embedding: 256 channels on a 64x64 grid.
inline constexpr std::size_t kSamEmbeddingValues = 256U * 64U * 64U;
// BiRefNet's fixed square input (the image is stretched to it, as in training).
inline constexpr std::int32_t kSubjectInputSize = 1024;

// Maps document pixels to the encoder's scaled frame: the longest side becomes
// kSamInputSize, aspect kept, anchored top-left.
struct SamTransform {
  std::int32_t source_width{0};
  std::int32_t source_height{0};
  std::int32_t scaled_width{0};
  std::int32_t scaled_height{0};
  double scale{1.0};
};

[[nodiscard]] SamTransform sam_transform(std::int32_t source_width, std::int32_t source_height) noexcept;

// A document coordinate (pixel corners at integers) in the encoder's frame.
struct SamPoint {
  float x{0.0F};
  float y{0.0F};
};
[[nodiscard]] SamPoint to_sam_point(const SamTransform& transform, double x, double y) noexcept;

// RGBA8 composited over white and resized to the encoder frame, as HWC floats in 0..255
// (the exported encoder normalizes and pads itself).
[[nodiscard]] std::vector<float> sam_encoder_input(const PixelBuffer& rgba8, const SamTransform& transform);

// RGBA8 composited over white, stretched to kSubjectInputSize square and normalized with
// the ImageNet mean/std, as NCHW floats.
[[nodiscard]] std::vector<float> subject_model_input(const PixelBuffer& rgba8);

// An 8-bit coverage mask positioned in document space. Empty when nothing is covered.
struct AiMask {
  PixelBuffer alpha;  // gray8, bounds.width x bounds.height
  Rect bounds{};
  [[nodiscard]] bool empty() const noexcept { return bounds.width <= 0 || bounds.height <= 0; }
};

// How a logit maps to coverage. SAM's decision boundary is logit 0 and its masks are hard,
// so it gets a fixed narrow ramp that anti-aliases the edge; BiRefNet predicts a matte, so
// it keeps the full sigmoid.
enum class LogitMapping {
  SamEdgeRamp,
  Sigmoid
};
[[nodiscard]] std::uint8_t logit_to_coverage(float logit, LogitMapping mapping) noexcept;

// Upsamples a logit grid that covers the whole document (model_width x model_height,
// stretched to the document) to document resolution with bilinear sampling, maps it to
// coverage, and crops to the covered bounds.
[[nodiscard]] AiMask document_mask_from_logits(const float* logits, std::int32_t model_width,
                                               std::int32_t model_height, std::int32_t document_width,
                                               std::int32_t document_height, LogitMapping mapping);

// Intersection over union of two masks at the 50% threshold, in document space. Two empty
// masks score 1.
[[nodiscard]] double mask_iou(const AiMask& a, const AiMask& b) noexcept;

}  // namespace patchy::ai
