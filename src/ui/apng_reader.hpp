#pragma once

// Animated PNG (APNG, GitHub issue 119): the PNG third edition's acTL / fcTL /
// fdAT chunks. Qt's PNG reader ignores them and returns the default image, so
// this reader splits the file into one standalone PNG per frame (the frame's
// IHDR, the file's palette and color chunks, its fdAT chunks rewritten as
// IDAT), decodes each through Qt, and composites them onto the canvas with the
// frame's dispose and blend operations. The result is full-canvas frames with
// millisecond delays, the shape the animated GIF and WebP imports hand to
// document_from_frames. docs/animation.md.

#include <QImage>

#include <cstdint>
#include <span>
#include <vector>

namespace patchy::apng {

struct Frame {
  QImage image;             // full canvas, ARGB32 premultiplied
  std::uint32_t delay_ms{};
};

struct Animation {
  int width{};
  int height{};
  std::uint32_t play_count{};  // acTL num_plays: 0 loops forever
  std::vector<Frame> frames;
};

// A PNG whose acTL (before the first IDAT) declares more than one frame.
[[nodiscard]] bool is_animated_png(std::span<const std::uint8_t> bytes) noexcept;

// Throws std::runtime_error (English, for the import notice) on a damaged
// animation: sequence numbers out of order, a frame outside the canvas, or a
// frame Qt cannot decode. A default image that is not part of the animation
// (IDAT with no fcTL before it) is skipped, as the specification requires.
[[nodiscard]] Animation read_animated_png(std::span<const std::uint8_t> bytes);

}  // namespace patchy::apng
