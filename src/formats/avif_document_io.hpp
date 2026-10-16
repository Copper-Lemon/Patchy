#pragma once

#include "formats/format_registry.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace patchy::avif {

// AVIF (AV1 Image File Format, .avif/.avifs): decode-only on every native platform through
// the vendored libavif 1.4.2 over the dav1d 1.5.4 AV1 decoder (src/formats/libavif,
// src/formats/dav1d; both BSD-2-Clause; cmake/patchy_avif.cmake). Design in docs/avif.md,
// the licensing record in docs/legal-constraints.md. The browser build compiles only the
// stub: is_available() is false there and read_avif() throws.
//
// The primary image opens as an 8-bit RGBA document, or 16-bit when the file stores 10 or
// 12-bit samples and deep editing is on. The clean aperture, rotation and mirror properties
// (clap, irot, imir) are applied in that order; an embedded ICC profile is kept on the
// document's color state. An image sequence (animated AVIF) becomes one full-canvas layer
// per frame, frame 1 on top, with the repetition count in kLoopCountMetadata. Patchy writes
// no AVIF: dav1d has no encoder.

inline constexpr const char* kLoopCountMetadata = "avif.loop_count";

// Lowercase extensions (no dot); the single source of truth for the registry, the dialog
// filter and the Windows "Open with" list.
[[nodiscard]] const std::vector<std::string>& avif_extensions();
[[nodiscard]] bool is_avif_extension(std::string_view extension);

// ISOBMFF ftyp whose major or compatible brand is avif or avis. HEVC HEIF brands belong to
// heif::sniff, never here.
[[nodiscard]] bool sniff(std::span<const std::uint8_t> bytes) noexcept;

// False in the browser build, where no AV1 decoder is compiled in.
[[nodiscard]] bool is_available() noexcept;

struct ReadOptions {
  // Names an animation frame layer; the UI passes translated "Frame N" text. Unset, frames
  // are named "Frame N <delay>s" in English.
  std::function<std::string(int index, std::uint32_t delay_ms)> frame_name;
};

struct ReadResult {
  FormatReadResult read;
  // True when the file carries an ICC profile: libavif hands over the file's own color
  // values, so that profile still describes them and is kept for saves.
  bool pixels_in_source_space{false};
  // The container's irot/imir properties were applied; the AVIF rules make them replace any
  // EXIF orientation, so the tag is written back as "normal".
  bool orientation_applied{true};
  std::uint32_t frame_count{1};
};

// Throws std::runtime_error with a PATCHY_TRANSLATE_NOOP message on a damaged, unsupported
// or oversized file; libavif's own English reason, when it gave one, follows a colon.
[[nodiscard]] ReadResult read_avif_detailed(std::span<const std::uint8_t> bytes, const ReadOptions& options = {});
// The registry's FormatReadFn.
[[nodiscard]] FormatReadResult read_avif(std::span<const std::uint8_t> bytes);

}  // namespace patchy::avif
