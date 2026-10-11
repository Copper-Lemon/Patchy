#pragma once

// Automatic selection inference (docs/ai-selection.md): MobileSAM for the Object
// Selection tool and BiRefNet for Select > Subject, both on ONNX Runtime's CPU provider.
// Builds without ONNX Runtime (PATCHY_ENABLE_AI_SELECTION off, wasm) link a stub that
// reports the feature unavailable, so callers never need their own #if.
//
// Patent boundary (binding; see the "Automatic selection" bullet in
// docs/legal-constraints.md): every call below is ONE network solve from ONE prompt.
// Never send a negative point (US 11314982), never combine a box with points in one solve
// (US 11568627), never feed a previous mask back into the decoder (US 11676279: the
// decoder's mask_input is always zero with has_mask_input 0), and Select Subject runs one
// pass over the whole image with no network-located crop or second pass (US 11244195).
// Callers combine results with plain selection booleans only.

#include "ai/ai_selection_math.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace patchy::ai {

// Thrown by an inference call whose InferenceCancel was requested.
class InferenceCancelled final : public std::runtime_error {
public:
  InferenceCancelled();
};

// Cancels a running inference from any thread. One object serves one call at a time.
class InferenceCancel {
public:
  void request();
  [[nodiscard]] bool requested() const noexcept;

  // Engine internals: the running call registers how to terminate its ONNX Runtime run,
  // so a request stops the run instead of waiting for it to finish. Attaching after a
  // request terminates at once.
  void attach(std::function<void()> terminate);
  void detach();

private:
  mutable std::mutex mutex_;
  std::atomic<bool> requested_{false};
  std::function<void()> terminate_;
};

// The models directory Patchy ships ("ai-models" beside the executable, or the bundle's
// Resources). Set once at startup, before the first call; tests point it at the build's
// copy. Thread-safe.
void set_model_directory(const std::filesystem::path& directory);
[[nodiscard]] std::filesystem::path model_directory();

// True when this build has ONNX Runtime and every model file is present.
[[nodiscard]] bool segmentation_available();
// English diagnostic for logs and script errors when unavailable; empty when available.
[[nodiscard]] std::string segmentation_unavailable_reason();

// The MobileSAM image embedding of one image. Computing it is the expensive step
// (about 0.2-0.8 s on a desktop CPU); each prompt afterwards is a fast decoder run.
struct SamEmbedding {
  std::vector<float> values;
  SamTransform transform;
};

// Encodes an RGBA8 image (composited over white). Throws InferenceCancelled on cancel and
// std::runtime_error when the model cannot run.
[[nodiscard]] std::shared_ptr<const SamEmbedding> sam_encode(const PixelBuffer& rgba8,
                                                             InferenceCancel* cancel = nullptr);

// One positive point (document coordinates) -> the object under it: one decoder run, the
// multimask output with the highest predicted IoU.
[[nodiscard]] AiMask sam_select_point(const SamEmbedding& embedding, double x, double y);

// One box (document coordinates) -> the object inside it: one decoder run, the single-mask
// output.
[[nodiscard]] AiMask sam_select_box(const SamEmbedding& embedding, Rect box);

// The main subject's soft matte at document resolution: one BiRefNet pass over the whole
// image (stretched to 1024 square), upsampled once.
[[nodiscard]] AiMask subject_matte(const PixelBuffer& rgba8, InferenceCancel* cancel = nullptr);

// Frees the loaded sessions (they reload on next use). The subject model is the large one
// (about 220 MB of weights, about 2 s to load): it stays loaded after a run so a second
// Select Subject is faster, and the app frees it after a while idle.
void release_models();
void release_subject_model();

}  // namespace patchy::ai
