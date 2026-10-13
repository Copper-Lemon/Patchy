#pragma once

// Runs G'MIC command lines on float images through the vendored interpreter
// (src/filters/gmic/gmic.cpp). Qt-free; the interpreter's own headers stay out
// of this one. The engine is built without external programs, network access,
// a display, or the patch-based algorithms. See docs/gmic.md.

#include <atomic>
#include <string>
#include <vector>

namespace patchy::gmic {

// One image in G'MIC's layout: planar float samples, channel after channel,
// each channel row by row, on a 0..255 scale for 8-bit data.
struct Image {
  int width{0};
  int height{0};
  int channels{0};
  std::vector<float> samples;
  // On output, the name the command gave the image (G'MIC filters encode layer
  // name, position, blend mode and opacity here).
  std::string name;

  [[nodiscard]] float& at(int x, int y, int channel) {
    return samples[(static_cast<std::size_t>(channel) * static_cast<std::size_t>(height) +
                    static_cast<std::size_t>(y)) *
                       static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)];
  }
  [[nodiscard]] float at(int x, int y, int channel) const {
    return samples[(static_cast<std::size_t>(channel) * static_cast<std::size_t>(height) +
                    static_cast<std::size_t>(y)) *
                       static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)];
  }
};

[[nodiscard]] Image make_image(int width, int height, int channels, float fill = 0.0F);

struct RunResult {
  bool ok{false};
  bool aborted{false};
  std::string error;   // the interpreter's message when !ok
  std::string status;  // the command's returned status (new parameter values for filters)
};

// Shared with a worker running Engine::run: set `abort` from any thread to stop
// it; `progress` is the percentage (0..100) a command last reported through
// G'MIC's `progress`, negative while it reports none. Many filters never report.
struct RunControl {
  std::atomic<bool> abort{false};
  std::atomic<float> progress{-1.0F};
};

class Engine {
public:
  // `commands` is the G'MIC command file the interpreter starts from (the
  // vendored standard library text). Nothing else is loaded. The interpreter
  // keeps one such library per process, so every Engine should be given the
  // same text; a run installs its engine's text before it starts.
  explicit Engine(std::string commands);

  // Runs one command line on `images`, which are replaced by the command's
  // output. Each call uses a fresh interpreter on a thread with the deep stack
  // G'MIC needs, and blocks until it ends. Never throws.
  [[nodiscard]] RunResult run(const std::string& command_line, std::vector<Image>& images,
                              RunControl* control = nullptr) const;

private:
  std::string commands_;
};

// False in builds without PATCHY_ENABLE_GMIC, where run() only reports that.
[[nodiscard]] bool engine_available() noexcept;

// "4.0.5": the vendored interpreter's version; empty when unavailable.
[[nodiscard]] std::string engine_version();

}  // namespace patchy::gmic
