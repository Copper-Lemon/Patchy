#pragma once

#include "core/document_file_info.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Reads every metadata block a flat image file carries into DocumentFileInfo
// (docs/file-info.md): JPEG, PNG, TIFF and TIFF-based raws, WebP, GIF, ISOBMFF
// (HEIF/HEIC/AVIF, Canon CR3), Fujifilm RAF, BMP, PDF and SVG. PSD/PSB metadata is
// lifted by the PSD reader instead (src/psd). Qt-free; tolerant of damaged input:
// a malformed block is skipped, never fatal.
namespace patchy::metadata {

// "JPEG", "PNG", "TIFF", "WebP", "GIF", "HEIF", "AVIF", "CR3", "RAF", "BMP", "PDF",
// "SVG", "PSD", or empty when unrecognized.
[[nodiscard]] std::string sniff_metadata_format(std::span<const std::uint8_t> bytes);

[[nodiscard]] DocumentFileInfo extract_file_info(std::span<const std::uint8_t> bytes);

// Reads the file (std::filesystem path, so Unicode names work) and extracts. Files
// larger than the read cap are scanned from their first bytes only. Never throws:
// an unreadable file returns an empty DocumentFileInfo.
[[nodiscard]] DocumentFileInfo extract_file_info_from_path(const std::filesystem::path& path);

// Decompresses a zlib stream, stopping at `max_output` bytes (zip-bomb guard).
// Empty on error.
[[nodiscard]] std::vector<std::uint8_t> inflate_zlib_capped(std::span<const std::uint8_t> data,
                                                            std::size_t max_output);

// True for an ICC profile header whose data color space is RGB.
[[nodiscard]] bool icc_profile_is_rgb(std::span<const std::uint8_t> profile) noexcept;
// The profile's data color space signature ("RGB ", "GRAY", "CMYK"), or empty.
[[nodiscard]] std::string icc_color_space(std::span<const std::uint8_t> profile);

}  // namespace patchy::metadata
