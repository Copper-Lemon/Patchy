#include "filters/gmic/gmic_engine.hpp"

#include "gmic.h"

#include <chrono>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <process.h>
#include <windows.h>
#elif !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <pthread.h>
#define PATCHY_GMIC_HAVE_PTHREAD 1
#endif

namespace patchy::gmic {

namespace {

// G'MIC commands recurse deeply (its own tools run with a 8 MB or larger
// stack); a default 1 MB thread stack overflows on ordinary filters.
constexpr std::size_t kInterpreterStackBytes = 32U * 1024U * 1024U;

#ifdef _WIN32
unsigned __stdcall run_work(void* work) {
  (*static_cast<std::function<void()>*>(work))();
  return 0;
}
#elif defined(PATCHY_GMIC_HAVE_PTHREAD)
void* run_work(void* work) {
  (*static_cast<std::function<void()>*>(work))();
  return nullptr;
}
#endif

// Runs `work` to completion on a thread with the interpreter's stack size, or
// inline where no such thread can be made. `work` must not throw.
void run_with_interpreter_stack(std::function<void()> work) {
#ifdef _WIN32
  const auto handle = _beginthreadex(nullptr, static_cast<unsigned>(kInterpreterStackBytes), run_work, &work,
                                     STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
  if (handle != 0) {
    WaitForSingleObject(reinterpret_cast<HANDLE>(handle), INFINITE);
    CloseHandle(reinterpret_cast<HANDLE>(handle));
    return;
  }
#elif defined(PATCHY_GMIC_HAVE_PTHREAD)
  pthread_attr_t attributes;
  if (pthread_attr_init(&attributes) == 0) {
    pthread_t thread;
    const bool started = pthread_attr_setstacksize(&attributes, kInterpreterStackBytes) == 0 &&
                         pthread_create(&thread, &attributes, run_work, &work) == 0;
    pthread_attr_destroy(&attributes);
    if (started) {
      pthread_join(thread, nullptr);
      return;
    }
  }
#endif
  work();
}

// The interpreter keeps one process-wide standard library, which every
// interpreter it creates internally (the math parser's run(), parallel
// commands) starts from as well. Patchy's stub leaves it empty, so the engine
// installs its command text there before the first run.
void install_standard_library(const std::string& commands) {
  static std::mutex mutex;
  static std::string installed;
  const std::scoped_lock lock(mutex);
  if (installed == commands && ::gmic::stdlib._data != nullptr) {
    return;
  }
  ::gmic::stdlib.assign(static_cast<unsigned int>(commands.size() + 1));
  std::memcpy(::gmic::stdlib._data, commands.c_str(), commands.size() + 1);
  installed = commands;
}

}  // namespace

bool engine_available() noexcept {
  return true;
}

Image make_image(int width, int height, int channels, float fill) {
  Image image;
  image.width = width;
  image.height = height;
  image.channels = channels;
  image.samples.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
                           static_cast<std::size_t>(channels),
                       fill);
  return image;
}

Engine::Engine(std::string commands) : commands_(std::move(commands)) {}

RunResult Engine::run(const std::string& command_line, std::vector<Image>& images, RunControl* control) const {
  RunResult result;
  install_standard_library(commands_);
  // The interpreter polls a plain bool and writes a plain float (its API); the
  // watcher below mirrors them to and from the caller's atomics.
  bool abort_flag = false;
  float progress = -1.0F;
  struct Watcher {
    std::atomic<bool> stop{false};
    std::thread thread;
    ~Watcher() {
      stop.store(true);
      if (thread.joinable()) {
        thread.join();
      }
    }
  } watcher;
  if (control != nullptr) {
    watcher.thread = std::thread([&abort_flag, &progress, &watcher, control] {
      while (!watcher.stop.load()) {
        if (control->abort.load(std::memory_order_relaxed)) {
          abort_flag = true;
        }
        control->progress.store(progress, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
      }
    });
  }
  run_with_interpreter_stack([&] {
    try {
      gmic_library::gmic_list<float> list;
      gmic_library::gmic_list<char> names;
      list.assign(static_cast<unsigned int>(images.size()));
      names.assign(static_cast<unsigned int>(images.size()));
      for (std::size_t index = 0; index < images.size(); ++index) {
        const Image& image = images[index];
        list[index].assign(static_cast<unsigned int>(image.width), static_cast<unsigned int>(image.height), 1U,
                           static_cast<unsigned int>(image.channels));
        std::memcpy(list[index]._data, image.samples.data(), image.samples.size() * sizeof(float));
        names[index].assign(static_cast<unsigned int>(image.name.size() + 1));
        std::memcpy(names[index]._data, image.name.c_str(), image.name.size() + 1);
      }
      ::gmic interpreter("", nullptr, true, &progress, &abort_flag, 0.0F);
      interpreter.run(command_line.c_str(), list, names);

      std::vector<Image> output;
      output.reserve(list._width);
      for (unsigned int index = 0; index < list._width; ++index) {
        const auto& source = list[index];
        // Only the first slice of a volume is an image layer.
        Image image = make_image(static_cast<int>(source._width), static_cast<int>(source._height),
                                 static_cast<int>(source._spectrum));
        const std::size_t plane = static_cast<std::size_t>(source._width) * source._height;
        const std::size_t volume = plane * source._depth;
        for (unsigned int channel = 0; channel < source._spectrum && source._data != nullptr; ++channel) {
          std::memcpy(image.samples.data() + plane * channel, source._data + volume * channel,
                      plane * sizeof(float));
        }
        if (index < names._width && names[index]._data != nullptr) {
          image.name = names[index]._data;
        }
        output.push_back(std::move(image));
      }
      images = std::move(output);
      if (interpreter.status._data != nullptr) {
        result.status = interpreter.status._data;
      }
      result.ok = true;
    } catch (const gmic_exception& error) {
      result.error = error.what();
    } catch (const std::exception& error) {
      result.error = error.what();
    } catch (...) {
      result.error = "Unknown G'MIC failure.";
    }
  });
  if (control != nullptr) {
    // The watcher samples; the value the run ended on is published exactly.
    control->progress.store(progress, std::memory_order_relaxed);
  }
  result.aborted = abort_flag;
  if (result.aborted) {
    result.ok = false;
  }
  return result;
}

std::string engine_version() {
  const int version = gmic_version;
  return std::to_string(version / 100) + "." + std::to_string(version / 10 % 10) + "." + std::to_string(version % 10);
}

}  // namespace patchy::gmic
