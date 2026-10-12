// Automatic selection in a build without an engine (PATCHY_ENABLE_AI_SELECTION off,
// wasm-core, the single-threaded wasm app): everything reports unavailable.

#include "ai/segmentation_engine.hpp"
#include "ai/segmentation_engine_internal.hpp"

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

bool subject_selection_available() {
  return false;
}

std::shared_ptr<SamEmbedding> encode_frame(const PixelBuffer&, InferenceCancel*) {
  throw_unavailable();
}

AiMask decode_prompt(const SamEmbedding&, const SamDecoderPrompt&) {
  throw_unavailable();
}

AiMask subject_matte(const PixelBuffer&, InferenceCancel*) {
  throw_unavailable();
}

int model_download_permille() noexcept {
  return -1;
}

void release_models() {}

void release_subject_model() {}

void release_high_quality_model() {}

}  // namespace patchy::ai
