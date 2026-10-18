#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#ifndef __EMSCRIPTEN__
#include <thread>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <crtdbg.h>
#include <windows.h>
#endif

namespace patchy::test {

using TestFn = std::function<void()>;

struct TestCase {
  std::string name;
  TestFn run;
};

inline void check(bool condition, const char* expression, const char* file, int line) {
  if (!condition) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + " check failed: " + expression);
  }
}

inline void suppress_crash_dialogs() noexcept {
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}

// Exit code of a run a test timeout ended (the same value as coreutils `timeout`).
inline constexpr int kTestTimeoutExitCode = 124;

// An environment variable's value, empty when unset (MSVC deprecates std::getenv).
inline std::string environment_value(const char* name) {
#ifdef _MSC_VER
  char* buffer = nullptr;
  std::size_t length = 0;
  std::string value;
  if (_dupenv_s(&buffer, &length, name) == 0 && buffer != nullptr) {
    value = buffer;
  }
  std::free(buffer);
  return value;
#else
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : std::string();
#endif
}

// The per-test limit in seconds: PATCHY_TEST_TIMEOUT_SECONDS when set (0 turns the
// watchdog off), else `fallback`. Debug builds get four times the fallback.
inline long long test_timeout_seconds(long long fallback) {
  if (const auto text = environment_value("PATCHY_TEST_TIMEOUT_SECONDS"); !text.empty()) {
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end != text.c_str() && value >= 0) {
      return value;
    }
  }
#ifndef NDEBUG
  fallback *= 4;
#endif
  return fallback;
}

// Fails a test that runs past its limit instead of letting the run hang (a modal
// dialog nobody answers sat for an hour on a build machine, October 2026). A thread
// waits on a deadline the runner arms per test; on expiry it prints a [FAIL] line
// naming the test, runs `report` for extra context, and ends the process at once
// with kTestTimeoutExitCode: the stuck test owns the main thread, so nothing could
// unwind it. `report` runs on the watchdog thread and must not touch GUI objects
// directly. Off under Emscripten (no threads there) and when the limit is 0.
class TestWatchdog {
public:
  using Report = std::function<void()>;

  TestWatchdog(long long limit_seconds, Report report = {}) : limit_seconds_(limit_seconds), report_(std::move(report)) {
#ifndef __EMSCRIPTEN__
    if (limit_seconds_ > 0) {
      thread_ = std::thread([this] { watch(); });
    }
#endif
  }

  ~TestWatchdog() {
#ifndef __EMSCRIPTEN__
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
    }
    changed_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
#endif
  }

  TestWatchdog(const TestWatchdog&) = delete;
  TestWatchdog& operator=(const TestWatchdog&) = delete;

  void begin(const std::string& name) {
    {
      std::lock_guard lock(mutex_);
      name_ = name;
      deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(limit_seconds_);
      armed_ = true;
    }
    changed_.notify_all();
  }

  void end() {
    {
      std::lock_guard lock(mutex_);
      armed_ = false;
    }
    changed_.notify_all();
  }

private:
#ifndef __EMSCRIPTEN__
  void watch() {
    std::unique_lock lock(mutex_);
    while (!stopping_) {
      if (!armed_) {
        changed_.wait(lock);
        continue;
      }
      const auto deadline = deadline_;
      if (changed_.wait_until(lock, deadline) != std::cv_status::timeout || !armed_ || deadline_ != deadline) {
        continue;
      }
      const auto name = name_;
      lock.unlock();
      std::fprintf(stderr, "[FAIL] %s: timed out after %lld s (PATCHY_TEST_TIMEOUT_SECONDS; 0 disables)\n",
                   name.c_str(), limit_seconds_);
      std::fflush(stderr);
      if (report_) {
        report_();
      }
      std::fflush(stdout);
      std::fflush(stderr);
      std::_Exit(kTestTimeoutExitCode);
    }
  }

  std::thread thread_;
#endif
  long long limit_seconds_;
  Report report_;
  std::mutex mutex_;
  std::condition_variable changed_;
  std::chrono::steady_clock::time_point deadline_{};
  std::string name_;
  bool armed_ = false;
  bool stopping_ = false;
};

}  // namespace patchy::test

#define CHECK(expression) ::patchy::test::check((expression), #expression, __FILE__, __LINE__)
