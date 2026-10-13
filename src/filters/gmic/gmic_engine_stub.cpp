// The engine in builds without PATCHY_ENABLE_GMIC: present so that callers link,
// and unavailable. Nothing here reaches the user: the G'MIC commands are only
// offered while engine_available() is true.

#include "filters/gmic/gmic_engine.hpp"

namespace patchy::gmic {

bool engine_available() noexcept {
  return false;
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

RunResult Engine::run(const std::string&, std::vector<Image>&, RunControl*) const {
  RunResult result;
  result.error = "This build does not include the G'MIC engine.";
  return result;
}

std::string engine_version() {
  return {};
}

}  // namespace patchy::gmic
