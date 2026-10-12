#pragma once

// Engine internals shared by the ONNX Runtime engine (desktop) and the web engine (wasm):
// how one prompt becomes decoder inputs and which decoder output is chosen. Both engines
// must solve identically, so this is the only place either rule lives. Read the patent
// boundary in segmentation_engine.hpp before changing anything here.

#include "ai/segmentation_engine.hpp"

#include <array>
#include <cstdint>

namespace patchy::ai {

// The multimask decoder returns four masks with predicted IoUs: index 0 is SAM's
// single-mask output, 1-3 the three alternatives a prompt can mean. A point picks among
// 1-3 (SAM's own rule for an ambiguous single point). A box picks among all four: MobileSAM's
// single-mask output degrades on a loose box (it pulled in a whole fridge behind a boy at
// predicted IoU 0.66 while alternative 3 was the boy at 0.96), so the decoder's own
// confidence decides. Either way it is one choice among the outputs of one run.
inline constexpr std::int64_t kDecoderMaskCount = 4;
inline constexpr std::int64_t kDecoderLowResSize = 256;

// Pinned by scripts/setup-ai-deps.* (docs/ai-selection.md lists provenance and licenses): the
// int8 conversions made by scripts/ai/quantize_models.py.
inline constexpr const char* kSamEncoderFile = "mobile_sam_image_encoder_int8.onnx";
inline constexpr const char* kSamDecoderFile = "sam_mask_decoder_multi_int8.onnx";
inline constexpr const char* kSubjectModelFile = "birefnet_lite_general_1024_int8.onnx";

// Exactly one prompt for one decoder run: a single positive point plus SAM's padding point,
// or a single box's two corners. The decoder's mask_input is always zero with
// has_mask_input 0; a previous mask is never fed back.
struct SamDecoderPrompt {
  std::array<float, 4> coords{};
  std::array<float, 2> labels{};
  std::int64_t first_candidate{0};
};

[[nodiscard]] SamDecoderPrompt sam_point_prompt(const SamTransform& transform, double x, double y) noexcept;
[[nodiscard]] SamDecoderPrompt sam_box_prompt(const SamTransform& transform, Rect box) noexcept;

// The output with the highest predicted IoU among first_candidate..kDecoderMaskCount-1.
[[nodiscard]] std::int64_t best_sam_candidate(const float* scores, std::int64_t first_candidate) noexcept;

}  // namespace patchy::ai
