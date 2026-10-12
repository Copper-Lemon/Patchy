// The parts of segmentation_engine.hpp shared by the ONNX Runtime engine, the web engine
// and the stub.

#include "ai/segmentation_engine.hpp"
#include "ai/segmentation_engine_internal.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <new>
#include <utility>

namespace patchy::ai {
namespace {

std::mutex& model_directory_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::filesystem::path& model_directory_storage() {
  static std::filesystem::path directory;
  return directory;
}

std::filesystem::path& downloaded_model_directory_storage() {
  static std::filesystem::path directory;
  return directory;
}

std::atomic<bool>& high_quality_enabled_storage() {
  static std::atomic<bool> enabled{true};
  return enabled;
}

}  // namespace

InferenceCancelled::InferenceCancelled() : std::runtime_error("automatic selection was cancelled") {}

InferenceOutOfMemory::InferenceOutOfMemory() : std::runtime_error("automatic selection ran out of memory") {}

bool is_out_of_memory_message(std::string_view message) noexcept {
  // std::bad_alloc::what() is "bad allocation" with MSVC and "std::bad_alloc" with libstdc++
  // and libc++; ONNX Runtime's allocators say "Failed to allocate memory".
  static constexpr std::array<std::string_view, 4> kMarkers{"bad allocation", "bad_alloc", "failed to allocate memory",
                                                            "out of memory"};
  // Compared in place: copying the message could itself fail when memory is short.
  const auto same_letter = [](char text, char marker) {
    return std::tolower(static_cast<unsigned char>(text)) == static_cast<unsigned char>(marker);
  };
  return std::any_of(kMarkers.begin(), kMarkers.end(), [&](std::string_view marker) {
    return std::search(message.begin(), message.end(), marker.begin(), marker.end(), same_letter) != message.end();
  });
}

bool is_out_of_memory(const std::exception& error) noexcept {
  return dynamic_cast<const InferenceOutOfMemory*>(&error) != nullptr ||
         dynamic_cast<const std::bad_alloc*>(&error) != nullptr;
}

void InferenceCancel::request() {
  requested_.store(true, std::memory_order_release);
  std::function<void()> terminate;
  {
    const std::lock_guard lock(mutex_);
    terminate = terminate_;
  }
  if (terminate) {
    terminate();
  }
}

bool InferenceCancel::requested() const noexcept {
  return requested_.load(std::memory_order_acquire);
}

void InferenceCancel::attach(std::function<void()> terminate) {
  {
    const std::lock_guard lock(mutex_);
    terminate_ = terminate;
  }
  if (requested() && terminate) {
    terminate();
  }
}

void InferenceCancel::detach() {
  const std::lock_guard lock(mutex_);
  terminate_ = nullptr;
}

void set_model_directory(const std::filesystem::path& directory) {
  const std::lock_guard lock(model_directory_mutex());
  model_directory_storage() = directory;
}

std::filesystem::path model_directory() {
  const std::lock_guard lock(model_directory_mutex());
  return model_directory_storage();
}

void set_downloaded_model_directory(const std::filesystem::path& directory) {
  const std::lock_guard lock(model_directory_mutex());
  downloaded_model_directory_storage() = directory;
}

std::filesystem::path downloaded_model_directory() {
  const std::lock_guard lock(model_directory_mutex());
  return downloaded_model_directory_storage();
}

std::filesystem::path high_quality_model_path(const char* file) {
  const auto downloaded = downloaded_model_directory();
  if (!downloaded.empty()) {
    auto candidate = downloaded / std::filesystem::path(file);
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return model_directory() / std::filesystem::path(file);
}

bool high_quality_model_available() {
#if defined(__EMSCRIPTEN__)
  return false;
#else
  for (const auto* file : {kEfficientSamEncoderFile, kEfficientSamDecoderFile}) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(high_quality_model_path(file), error)) {
      return false;
    }
  }
  return true;
#endif
}

void set_high_quality_model_enabled(bool enabled) {
  high_quality_enabled_storage().store(enabled, std::memory_order_release);
}

bool high_quality_model_enabled() {
  return high_quality_enabled_storage().load(std::memory_order_acquire);
}

SamModel active_sam_model() {
  return high_quality_model_enabled() && high_quality_model_available() ? SamModel::HighQuality : SamModel::Standard;
}

SamDecoderPrompt sam_point_prompt(const SamTransform& transform, double x, double y) noexcept {
  const auto point = to_sam_point(transform, x, y);
  // Label 1 is the one positive point; label -1 is SAM's required padding point when no
  // box is given. Never a 0 (negative) label.
  return {{point.x, point.y, 0.0F, 0.0F}, {1.0F, -1.0F}, 1};
}

SamDecoderPrompt sam_box_prompt(const SamTransform& transform, Rect box) noexcept {
  const auto top_left = to_sam_point(transform, box.x, box.y);
  const auto bottom_right = to_sam_point(transform, box.x + box.width, box.y + box.height);
  // Labels 2 and 3 are the box corners; no point rides along.
  return {{top_left.x, top_left.y, bottom_right.x, bottom_right.y}, {2.0F, 3.0F}, 0};
}

std::int64_t best_sam_candidate(const float* scores, std::int64_t first_candidate) noexcept {
  std::int64_t chosen = first_candidate;
  for (std::int64_t index = first_candidate + 1; index < kDecoderMaskCount; ++index) {
    if (scores[index] > scores[chosen]) {
      chosen = index;
    }
  }
  return chosen;
}

std::shared_ptr<const SamEmbedding> sam_encode(const PixelBuffer& rgba8, InferenceCancel* cancel) {
  return encode_frame(rgba8, cancel);
}

std::shared_ptr<const SamEmbedding> sam_encode_region(const PixelBuffer& rgba8, Rect region, InferenceCancel* cancel) {
  const auto crop = crop_rgba(rgba8, region);
  auto embedding = encode_frame(crop, cancel);
  // crop_rgba clamped the region to the image; the crop's corner is its origin.
  embedding->transform.origin_x = std::clamp(region.x, 0, rgba8.width());
  embedding->transform.origin_y = std::clamp(region.y, 0, rgba8.height());
  return embedding;
}

AiMask sam_select_point(const SamEmbedding& embedding, double x, double y) {
  const auto& transform = embedding.transform;
  return translated_mask(decode_prompt(embedding, sam_point_prompt(transform, x, y)), transform.origin_x,
                         transform.origin_y);
}

AiMask sam_select_box(const SamEmbedding& embedding, Rect box) {
  const auto& transform = embedding.transform;
  // The result stays inside the box.
  return clip_mask_to_rect(translated_mask(decode_prompt(embedding, sam_box_prompt(transform, box)),
                                           transform.origin_x, transform.origin_y),
                           box);
}

}  // namespace patchy::ai
