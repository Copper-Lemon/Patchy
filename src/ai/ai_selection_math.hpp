#pragma once

// Pre- and post-processing for the automatic-selection models (docs/ai-selection.md).
// Qt-free and independent of ONNX Runtime, so every build compiles and tests it. The
// model outputs these functions consume are CPU-dependent float math, so callers and
// tests compare masks by IoU and never pin bytes.

#include "core/layer.hpp"
#include "core/pixel_buffer.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace patchy::ai {

// The longest image side both SAM encoders take (MobileSAM pads the rest itself; Patchy pads
// EfficientViT-SAM's input to a 1024 square).
inline constexpr std::int32_t kSamInputSize = 1024;
// The SAM image embedding of both encoders: 256 channels on a 64x64 grid.
inline constexpr std::size_t kSamEmbeddingValues = 256U * 64U * 64U;
// SAM decoders compute their masks on a 256x256 grid over the padded 1024 frame.
inline constexpr std::int32_t kSamLowResSize = 256;
// BiRefNet's fixed square input (the image is stretched to it, as in training).
inline constexpr std::int32_t kSubjectInputSize = 1024;

// Maps document pixels to the encoder's scaled frame: the longest side of the encoded
// region becomes kSamInputSize, aspect kept, anchored top-left. The region is the whole
// image (origin 0,0) or, for zoom-to-box, the part sam_box_region chose.
struct SamTransform {
  std::int32_t source_width{0};  // the encoded region's size in document pixels
  std::int32_t source_height{0};
  std::int32_t scaled_width{0};
  std::int32_t scaled_height{0};
  double scale{1.0};
  std::int32_t origin_x{0};  // the encoded region's top-left in the document
  std::int32_t origin_y{0};
};

[[nodiscard]] SamTransform sam_transform(std::int32_t source_width, std::int32_t source_height) noexcept;
// The transform for encoding only `region` of a document.
[[nodiscard]] SamTransform sam_region_transform(Rect region) noexcept;

// Zoom-to-box (docs/ai-selection.md): the part of a width x height image a box prompt is
// encoded from, so the object fills the encoder's frame instead of a corner of it. The box
// grows by a fixed margin on every side (kSamBoxMarginPercent of its longer side, at least
// kSamBoxMinimumMargin pixels), widens to at least kSamBoxMinimumRegion pixels, and is
// clamped to the image. Pure geometry from the user's box: never derived from pixel
// content or a model output (patent boundary in segmentation_engine.hpp). nullopt when the
// region would cover most of the image anyway, so the whole-image embedding serves.
inline constexpr std::int32_t kSamBoxMarginPercent = 15;
inline constexpr std::int32_t kSamBoxMinimumMargin = 64;
inline constexpr std::int32_t kSamBoxMinimumRegion = 384;
inline constexpr std::int32_t kSamBoxWholeImagePercent = 80;
[[nodiscard]] std::optional<Rect> sam_box_region(Rect box, std::int32_t width, std::int32_t height) noexcept;

// A copy of `region` (clamped to the image) of an RGBA8 image.
[[nodiscard]] PixelBuffer crop_rgba(const PixelBuffer& rgba8, Rect region);

// A document coordinate (pixel corners at integers) in the encoder's frame.
struct SamPoint {
  float x{0.0F};
  float y{0.0F};
};
[[nodiscard]] SamPoint to_sam_point(const SamTransform& transform, double x, double y) noexcept;

// RGBA8 composited over white and resized to the encoder frame, as HWC floats in 0..255
// (the MobileSAM export normalizes and pads itself).
[[nodiscard]] std::vector<float> sam_encoder_input(const PixelBuffer& rgba8, const SamTransform& transform);

// RGBA8 composited over white, resized to the encoder frame, normalized with the ImageNet
// mean/std and zero-padded to a kSamInputSize square, as NCHW floats (EfficientViT-SAM).
[[nodiscard]] std::vector<float> padded_sam_encoder_input(const PixelBuffer& rgba8, const SamTransform& transform);

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

// The same for a SAM decoder's own kSamLowResSize grid over the padded 1024 frame
// (row-major, kSamLowResSize wide): one bilinear upsample straight to the encoded region's
// document pixels, padding ignored. The mask is in region coordinates; see translated_mask.
[[nodiscard]] AiMask document_mask_from_low_res_logits(const float* logits, const SamTransform& transform);

// `mask` moved by (dx, dy) document pixels.
[[nodiscard]] AiMask translated_mask(AiMask mask, std::int32_t dx, std::int32_t dy) noexcept;

// Clears coverage outside `rect` (document coordinates) and crops to what remains. A box
// prompt means "the object inside this box"; SAM's masks can spill past the box edges.
[[nodiscard]] AiMask clip_mask_to_rect(const AiMask& mask, Rect rect);

// Intersection over union of two masks at the 50% threshold, in document space. Two empty
// masks score 1.
[[nodiscard]] double mask_iou(const AiMask& a, const AiMask& b) noexcept;

}  // namespace patchy::ai
