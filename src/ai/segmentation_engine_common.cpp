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

}  // namespace patchy::ai
