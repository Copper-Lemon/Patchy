// Automatic selection in a build without ONNX Runtime (PATCHY_ENABLE_AI_SELECTION off,
// including every wasm build for now): everything reports unavailable.

#include "ai/segmentation_engine.hpp"

namespace patchy::ai {
namespace {

[[noreturn]] void throw_unavailable() {
  throw std::runtime_error(segmentation_unavailable_reason());
}

}  // namespace

bool segmentation_available() {
  return false;
}

std::string segmentation_unavailable_reason() {
  return "this build of Patchy does not include automatic selection";
}

std::shared_ptr<const SamEmbedding> sam_encode(const PixelBuffer&, InferenceCancel*) {
  throw_unavailable();
}

AiMask sam_select_point(const SamEmbedding&, double, double) {
  throw_unavailable();
}

AiMask sam_select_box(const SamEmbedding&, Rect) {
  throw_unavailable();
}

AiMask subject_matte(const PixelBuffer&, InferenceCancel*) {
  throw_unavailable();
}

void release_models() {}

void release_subject_model() {}

}  // namespace patchy::ai
