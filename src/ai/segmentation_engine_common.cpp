// The parts of segmentation_engine.hpp shared by the ONNX Runtime engine and the stub.

#include "ai/segmentation_engine.hpp"

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

}  // namespace patchy::ai
