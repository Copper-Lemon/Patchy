#pragma once

#include <cstdint>

namespace patchy {

// The luminance weights the Grayscale filter uses (30/59/11, integer, truncating), shared by
// every "make this gray" path (the PDF import's Grayscale mode, raster and editable), so a
// gray import and a desaturated layer agree to the byte.
[[nodiscard]] constexpr std::uint8_t gray_from_rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept {
  return static_cast<std::uint8_t>((static_cast<int>(red) * 30 + static_cast<int>(green) * 59 +
                                    static_cast<int>(blue) * 11) /
                                   100);
}

}  // namespace patchy
