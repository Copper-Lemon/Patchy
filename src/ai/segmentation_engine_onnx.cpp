// Automatic selection on ONNX Runtime's CPU execution provider. This is the only
// translation unit that includes ONNX Runtime headers. Read the patent boundary in
// segmentation_engine.hpp before changing any prompt or decoder input here.
//
// ONNX Runtime is loaded at run time, never linked: a machine where the library cannot
// load (macOS older than the dylib's minimum, a damaged install) still starts Patchy and
// simply reports automatic selection unavailable.

#include "ai/segmentation_engine.hpp"

#include "core/environment.hpp"
#include "core/worker_budget.hpp"

#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#undef ORT_API_MANUAL_INIT

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace patchy::ai {
namespace {

// Pinned by scripts/setup-ai-deps.* (docs/ai-selection.md lists provenance and licenses): the
// int8 conversions made by scripts/ai/quantize_models.py.
constexpr const char* kSamEncoderFile = "mobile_sam_image_encoder_int8.onnx";
constexpr const char* kSamDecoderFile = "sam_mask_decoder_multi_int8.onnx";
constexpr const char* kSubjectModelFile = "birefnet_lite_general_1024_int8.onnx";

// The multimask decoder returns four masks with predicted IoUs: index 0 is SAM's
// single-mask output, 1-3 the three alternatives a prompt can mean. A point picks among
// 1-3 (SAM's own rule for an ambiguous single point). A box picks among all four: MobileSAM's
// single-mask output degrades on a loose box (it pulled in a whole fridge behind a boy at
// predicted IoU 0.66 while alternative 3 was the boy at 0.96), so the decoder's own
// confidence decides. Either way it is one choice among the outputs of one run.
constexpr std::int64_t kDecoderMaskCount = 4;
constexpr std::int64_t kDecoderLowResSize = 256;

// The executable's folder, where build trees and the Windows package keep the library.
std::filesystem::path executable_directory() {
#if defined(_WIN32)
  std::vector<wchar_t> buffer(32768);
  const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  return length == 0 ? std::filesystem::path() : std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#elif defined(__APPLE__)
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size + 1U, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    return {};
  }
  return std::filesystem::path(buffer.data()).parent_path();
#else
  std::error_code error;
  const auto self = std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::filesystem::path() : self.parent_path();
#endif
}

using GetApiBaseFunction = const OrtApiBase* (*)();

// Loads the library once and initializes the C++ API from it. The handle is never closed.
struct RuntimeLoad {
  bool loaded{false};
  std::string error;
};

const RuntimeLoad& runtime() {
  static const RuntimeLoad result = [] {
    RuntimeLoad load;
    GetApiBaseFunction get_api_base = nullptr;
    const auto directory = executable_directory();
#if defined(_WIN32)
    // Full path only: System32 can hold an unrelated older onnxruntime.dll.
    const auto library = directory / PATCHY_ONNXRUNTIME_LIBRARY;
    if (auto* module = LoadLibraryExW(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH); module != nullptr) {
      get_api_base = reinterpret_cast<GetApiBaseFunction>(
          reinterpret_cast<void*>(GetProcAddress(module, "OrtGetApiBase")));
    }
#else
    std::vector<std::filesystem::path> candidates{directory / PATCHY_ONNXRUNTIME_LIBRARY,
                                                  directory / ".." / "Frameworks" / PATCHY_ONNXRUNTIME_LIBRARY,
                                                  std::filesystem::path(PATCHY_ONNXRUNTIME_LIBRARY)};
    for (const auto& candidate : candidates) {
      if (void* handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL); handle != nullptr) {
        get_api_base = reinterpret_cast<GetApiBaseFunction>(dlsym(handle, "OrtGetApiBase"));
        break;
      }
      const char* message = dlerror();
      load.error = message != nullptr ? message : "dlopen failed";
    }
#endif
    const OrtApi* api = get_api_base != nullptr ? get_api_base()->GetApi(ORT_API_VERSION) : nullptr;
    if (api == nullptr) {
      if (load.error.empty()) {
        load.error = std::string("ONNX Runtime (") + PATCHY_ONNXRUNTIME_LIBRARY + ") could not be loaded";
      } else {
        load.error = "ONNX Runtime could not be loaded: " + load.error;
      }
      return load;
    }
    Ort::InitApi(api);
    load.loaded = true;
    load.error.clear();
    return load;
  }();
  return result;
}

struct OrtState {
  std::mutex mutex;
  // Sessions must not outlive their environment; the state is never destroyed (see
  // ort_state) and release_models() only resets sessions.
  std::optional<Ort::Env> env;
  std::unique_ptr<Ort::Session> encoder;
  std::unique_ptr<Ort::Session> decoder;
  std::unique_ptr<Ort::Session> subject;
};

// Never destroyed: at process exit ONNX Runtime's own globals can be torn down before ours
// (seen on macOS, where ReleaseEnv then aborts on a destroyed mutex), so the environment and
// sessions are left for the OS to reclaim. release_models() frees the sessions on demand.
OrtState& ort_state() {
  static auto* state = new OrtState;
  return *state;
}

Ort::Env& environment(OrtState& state) {
  if (!runtime().loaded) {
    throw std::runtime_error(runtime().error);
  }
  if (!state.env) {
    state.env.emplace(ORT_LOGGING_LEVEL_ERROR, "patchy");
  }
  return *state.env;
}

std::filesystem::path model_path(const char* file) {
  return model_directory() / std::filesystem::path(file);
}

Ort::SessionOptions session_options() {
  Ort::SessionOptions options;
  // ONNX Runtime sizes its pool to the physical cores by default, which measured fastest.
  // PATCHY_RENDER_THREADS still emulates a smaller machine (core/worker_budget.hpp).
  if (environment_variable_is_set("PATCHY_RENDER_THREADS")) {
    options.SetIntraOpNumThreads(hardware_worker_threads());
  }
  options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  // Without the arena, activation memory goes back to the system after each run instead
  // of staying reserved for the life of the session.
  options.DisableCpuMemArena();
  return options;
}

std::unique_ptr<Ort::Session> open_session(Ort::Env& env, const char* file) {
  const auto path = model_path(file);
  try {
    return std::make_unique<Ort::Session>(env, path.c_str(), session_options());
  } catch (const Ort::Exception& error) {
    throw std::runtime_error(std::string("automatic selection could not load ") + file + ": " + error.what());
  }
}

Ort::Session& shared_session(OrtState& state, std::unique_ptr<Ort::Session>& slot, const char* file) {
  if (!slot) {
    slot = open_session(environment(state), file);
  }
  return *slot;
}

// Runs a session with cancellation wired to `cancel`; translates ONNX Runtime failures.
std::vector<Ort::Value> run(Ort::Session& session, const char* const* input_names, const Ort::Value* inputs,
                            std::size_t input_count, const char* const* output_names, std::size_t output_count,
                            InferenceCancel* cancel) {
  Ort::RunOptions run_options;
  if (cancel != nullptr) {
    if (cancel->requested()) {
      throw InferenceCancelled();
    }
    cancel->attach([&run_options] { run_options.SetTerminate(); });
  }
  try {
    auto outputs = session.Run(run_options, input_names, inputs, input_count, output_names, output_count);
    if (cancel != nullptr) {
      cancel->detach();
    }
    return outputs;
  } catch (const Ort::Exception& error) {
    if (cancel != nullptr) {
      cancel->detach();
      if (cancel->requested()) {
        throw InferenceCancelled();
      }
    }
    throw std::runtime_error(std::string("automatic selection failed: ") + error.what());
  }
}

Ort::MemoryInfo cpu_memory() {
  return Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
}

// One decoder run. `coords`/`labels` are exactly one prompt: a single positive point plus
// SAM's padding point, or a single box's two corners. mask_input stays zero with
// has_mask_input 0 on every call: a previous mask is never fed back (patent boundary in
// segmentation_engine.hpp).
AiMask decode(const SamEmbedding& embedding, std::array<float, 4> coords, std::array<float, 2> labels,
              std::int64_t first_candidate) {
  if (embedding.values.size() != kSamEmbeddingValues) {
    throw std::invalid_argument("automatic selection: malformed image embedding");
  }
  auto& state = ort_state();
  Ort::Session* session = nullptr;
  {
    const std::lock_guard lock(state.mutex);
    session = &shared_session(state, state.decoder, kSamDecoderFile);
  }
  const auto& transform = embedding.transform;
  auto memory = cpu_memory();
  std::vector<float> embedding_values = embedding.values;
  std::vector<float> no_mask(static_cast<std::size_t>(kDecoderLowResSize * kDecoderLowResSize), 0.0F);
  float has_mask = 0.0F;
  std::array<float, 2> output_size{static_cast<float>(transform.scaled_height),
                                   static_cast<float>(transform.scaled_width)};
  const std::array<std::int64_t, 4> embedding_shape{1, 256, 64, 64};
  const std::array<std::int64_t, 3> coords_shape{1, 2, 2};
  const std::array<std::int64_t, 2> labels_shape{1, 2};
  const std::array<std::int64_t, 4> mask_shape{1, 1, kDecoderLowResSize, kDecoderLowResSize};
  const std::array<std::int64_t, 1> flag_shape{1};
  const std::array<std::int64_t, 1> size_shape{2};
  std::array<Ort::Value, 6> inputs{
      Ort::Value::CreateTensor<float>(memory, embedding_values.data(), embedding_values.size(),
                                      embedding_shape.data(), embedding_shape.size()),
      Ort::Value::CreateTensor<float>(memory, coords.data(), coords.size(), coords_shape.data(),
                                      coords_shape.size()),
      Ort::Value::CreateTensor<float>(memory, labels.data(), labels.size(), labels_shape.data(),
                                      labels_shape.size()),
      Ort::Value::CreateTensor<float>(memory, no_mask.data(), no_mask.size(), mask_shape.data(), mask_shape.size()),
      Ort::Value::CreateTensor<float>(memory, &has_mask, 1, flag_shape.data(), flag_shape.size()),
      Ort::Value::CreateTensor<float>(memory, output_size.data(), output_size.size(), size_shape.data(),
                                      size_shape.size()),
  };
  constexpr std::array<const char*, 6> input_names{"image_embeddings", "point_coords", "point_labels",
                                                   "mask_input",       "has_mask_input", "orig_im_size"};
  constexpr std::array<const char*, 2> output_names{"masks", "iou_predictions"};
  auto outputs = run(*session, input_names.data(), inputs.data(), inputs.size(), output_names.data(),
                     output_names.size(), nullptr);

  const auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
  if (shape.size() != 4 || shape[1] != kDecoderMaskCount || shape[2] != transform.scaled_height ||
      shape[3] != transform.scaled_width) {
    throw std::runtime_error("automatic selection: unexpected decoder output shape");
  }
  const float* scores = outputs[1].GetTensorData<float>();
  std::int64_t chosen = first_candidate;
  for (std::int64_t index = first_candidate + 1; index < kDecoderMaskCount; ++index) {
    if (scores[index] > scores[chosen]) {
      chosen = index;
    }
  }
  const auto plane = static_cast<std::size_t>(transform.scaled_width) * static_cast<std::size_t>(transform.scaled_height);
  const float* logits = outputs[0].GetTensorData<float>() + static_cast<std::size_t>(chosen) * plane;
  return document_mask_from_logits(logits, transform.scaled_width, transform.scaled_height, transform.source_width,
                                   transform.source_height, LogitMapping::SamEdgeRamp);
}

}  // namespace

bool segmentation_available() {
  return segmentation_unavailable_reason().empty();
}

std::string segmentation_unavailable_reason() {
  if (!runtime().loaded) {
    return runtime().error;
  }
  const auto directory = model_directory();
  if (directory.empty()) {
    return "the automatic selection models folder is not set";
  }
  for (const auto* file : {kSamEncoderFile, kSamDecoderFile, kSubjectModelFile}) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(model_path(file), error)) {
      return std::string("the automatic selection model ") + file + " is missing";
    }
  }
  return {};
}

std::shared_ptr<const SamEmbedding> sam_encode(const PixelBuffer& rgba8, InferenceCancel* cancel) {
  auto& state = ort_state();
  Ort::Session* session = nullptr;
  {
    const std::lock_guard lock(state.mutex);
    session = &shared_session(state, state.encoder, kSamEncoderFile);
  }
  auto embedding = std::make_shared<SamEmbedding>();
  embedding->transform = sam_transform(rgba8.width(), rgba8.height());
  auto input = sam_encoder_input(rgba8, embedding->transform);
  const std::array<std::int64_t, 3> shape{embedding->transform.scaled_height, embedding->transform.scaled_width, 3};
  auto memory = cpu_memory();
  auto tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), shape.data(), shape.size());
  constexpr std::array<const char*, 1> input_names{"input_image"};
  constexpr std::array<const char*, 1> output_names{"image_embeddings"};
  auto outputs = run(*session, input_names.data(), &tensor, 1, output_names.data(), 1, cancel);
  const float* values = outputs[0].GetTensorData<float>();
  if (outputs[0].GetTensorTypeAndShapeInfo().GetElementCount() != kSamEmbeddingValues) {
    throw std::runtime_error("automatic selection: unexpected encoder output shape");
  }
  embedding->values.assign(values, values + kSamEmbeddingValues);
  return embedding;
}

AiMask sam_select_point(const SamEmbedding& embedding, double x, double y) {
  const auto point = to_sam_point(embedding.transform, x, y);
  // Label 1 is the one positive point; label -1 is SAM's required padding point when no
  // box is given. Never a 0 (negative) label.
  return decode(embedding, {point.x, point.y, 0.0F, 0.0F}, {1.0F, -1.0F}, 1);
}

AiMask sam_select_box(const SamEmbedding& embedding, Rect box) {
  const auto top_left = to_sam_point(embedding.transform, box.x, box.y);
  const auto bottom_right = to_sam_point(embedding.transform, box.x + box.width, box.y + box.height);
  // Labels 2 and 3 are the box corners; no point rides along. The result stays inside the box.
  return clip_mask_to_rect(
      decode(embedding, {top_left.x, top_left.y, bottom_right.x, bottom_right.y}, {2.0F, 3.0F}, 0), box);
}

AiMask subject_matte(const PixelBuffer& rgba8, InferenceCancel* cancel) {
  Ort::Session* session = nullptr;
  {
    auto& state = ort_state();
    const std::lock_guard lock(state.mutex);
    session = &shared_session(state, state.subject, kSubjectModelFile);
  }
  if (cancel != nullptr && cancel->requested()) {
    throw InferenceCancelled();
  }
  auto input = subject_model_input(rgba8);
  const std::array<std::int64_t, 4> shape{1, 3, kSubjectInputSize, kSubjectInputSize};
  auto memory = cpu_memory();
  auto tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), shape.data(), shape.size());
  constexpr std::array<const char*, 1> input_names{"input_image"};
  constexpr std::array<const char*, 1> output_names{"output_image"};
  auto outputs = run(*session, input_names.data(), &tensor, 1, output_names.data(), 1, cancel);
  if (outputs[0].GetTensorTypeAndShapeInfo().GetElementCount() !=
      static_cast<std::size_t>(kSubjectInputSize) * static_cast<std::size_t>(kSubjectInputSize)) {
    throw std::runtime_error("automatic selection: unexpected subject model output shape");
  }
  // One pass over the whole image, upsampled once: no crop or second pass (patent boundary
  // in segmentation_engine.hpp).
  return document_mask_from_logits(outputs[0].GetTensorData<float>(), kSubjectInputSize, kSubjectInputSize,
                                   rgba8.width(), rgba8.height(), LogitMapping::Sigmoid);
}

void release_models() {
  auto& state = ort_state();
  const std::lock_guard lock(state.mutex);
  state.encoder.reset();
  state.decoder.reset();
  state.subject.reset();
}

void release_subject_model() {
  auto& state = ort_state();
  const std::lock_guard lock(state.mutex);
  state.subject.reset();
}

}  // namespace patchy::ai
