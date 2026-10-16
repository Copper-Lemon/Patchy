#pragma once

#include "formats/format_registry.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace patchy::exr {

// OpenEXR (.exr): ILM's HDR interchange format, linear scene-referred floats in half,
// float or uint channels with per-tile or per-scanline compression. Patchy reads a
// single-part RGB(A) or Y image through the vendored tinyexr (src/formats/tinyexr,
// BSD-3; NONE, RLE, ZIPS, ZIP and PIZ compression, scanlines and tiles) into a 32-bit
// floating-point document, and writes the flattened document as float RGBA with ZIP
// compression. Deep images, multi-part files and the B44/DWA codecs are not read
// (docs/hdr-formats.md).

[[nodiscard]] const std::vector<std::string>& exr_extensions();
[[nodiscard]] bool is_exr_extension(std::string_view extension);
[[nodiscard]] bool sniff(std::span<const std::uint8_t> bytes) noexcept;

// Throws std::runtime_error with a PATCHY_TRANSLATE_NOOP message when tinyexr rejects
// the file; tinyexr's own English reason is appended after a colon.
[[nodiscard]] FormatReadResult read_exr(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<std::uint8_t> write_exr(const Document& document);

}  // namespace patchy::exr
