#include "ui/save_progress.hpp"

#include <utility>

namespace patchy::ui {
namespace {

thread_local SaveProgress* active_save_progress = nullptr;

}  // namespace

void SaveProgress::report(int done, int total, Kind kind, QString label) {
  {
    const std::lock_guard lock(mutex_);
    state_.done = done;
    state_.total = total;
    state_.kind = kind;
    state_.label = std::move(label);
  }
  if (pump_) {
    pump_();
  }
}

SaveProgress::State SaveProgress::state() const {
  const std::lock_guard lock(mutex_);
  return state_;
}

ScopedSaveProgress::ScopedSaveProgress(SaveProgress& progress) noexcept : previous_(active_save_progress) {
  active_save_progress = &progress;
}

ScopedSaveProgress::~ScopedSaveProgress() {
  active_save_progress = previous_;
}

void report_save_progress(int done, int total, SaveProgress::Kind kind, const QString& label) {
  if (active_save_progress != nullptr) {
    active_save_progress->report(done, total, kind, label);
  }
}

}  // namespace patchy::ui
