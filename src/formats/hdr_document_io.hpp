#pragma once

#include "formats/format_registry.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace patchy::hdr {

// Radiance RGBE (.hdr, .rgbe): Greg Ward's 1980s HDR picture format, still what
// panoramic light probes and many renderers write. A text header ("#?RADIANCE" or
// "#?RGBE", FORMAT=32-bit_rle_rgbe, optional EXPOSURE), a resolution line ("-Y h +X w"
// is the standard orientation), then one RGBE quadruple per pixel: three mantissas
// and a shared exponent, flat for narrow images and run-length coded per channel
// ("new RLE", 2 2 hi lo) otherwise. Values are linear radiance, so Patchy opens the
// file as a 32-bit floating-point document (docs/high-bit-depth.md). Patchy's own
// code, no third-party library (docs/hdr-formats.md).

[[nodiscard]] const std::vector<std::string>& hdr_extensions();
[[nodiscard]] bool is_hdr_extension(std::string_view extension);
[[nodiscard]] bool sniff(std::span<const std::uint8_t> bytes) noexcept;

// Throws std::runtime_error with a PATCHY_TRANSLATE_NOOP message on a damaged or
// unsupported file (XYZE color, an orientation other than top-down rows).
[[nodiscard]] FormatReadResult read_hdr(std::span<const std::uint8_t> bytes);

// Writes the flattened document as 32-bit_rle_rgbe with new-style run-length scanlines
// (flat for widths under 8 or over 32767, as the format requires). An 8 or 16-bit
// document is written as its linear light values; a 32-bit one as its radiance.
[[nodiscard]] std::vector<std::uint8_t> write_hdr(const Document& document);

// RGBE helpers, shared with tests: mantissa/exponent to linear float and back.
void rgbe_to_float(const std::uint8_t rgbe[4], float out_rgb[3]) noexcept;
void float_to_rgbe(const float rgb[3], std::uint8_t out_rgbe[4]) noexcept;

}  // namespace patchy::hdr
