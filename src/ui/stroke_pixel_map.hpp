#pragma once

// Per-pixel state of one paint stroke (opacity caps, accumulated alpha, the
// Wet Edges footprint), keyed by document pixel. Brush strokes look a pixel up
// for every pixel of every dab, so this is a sparse grid of dense 64x64 tiles
// with a one-tile cache rather than a hash map per pixel: an unordered_map
// lookup per pixel made a 300 px brush cost about 0.1 ms per document pixel of
// travel, which stalled the GUI thread between mouse moves (GitHub issue 165).
// Same semantics as the map it replaces: absent until written, a fresh entry
// starts at T{}.

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace patchy::ui {

template <typename T>
class StrokePixelMap {
 public:
  // The entry at (x, y), or nullptr when the stroke never wrote it.
  [[nodiscard]] T* find(std::int32_t x, std::int32_t y) noexcept {
    auto* tile = tile_at(x, y, false);
    if (tile == nullptr) {
      return nullptr;
    }
    const auto index = index_in_tile(x, y);
    return tile->present[index] ? &tile->values[index] : nullptr;
  }

  // The entry at (x, y), created as T{} when absent (unordered_map::operator[]).
  T& operator()(std::int32_t x, std::int32_t y) {
    auto* tile = tile_at(x, y, true);
    const auto index = index_in_tile(x, y);
    if (!tile->present[index]) {
      tile->present[index] = true;
      tile->values[index] = T{};
    }
    return tile->values[index];
  }

  // Marks (x, y) present; true when it was absent (unordered_set::insert().second).
  bool insert(std::int32_t x, std::int32_t y) {
    auto* tile = tile_at(x, y, true);
    const auto index = index_in_tile(x, y);
    if (tile->present[index]) {
      return false;
    }
    tile->present[index] = true;
    tile->values[index] = T{};
    return true;
  }

  void clear() noexcept {
    tiles_.clear();
    cached_tile_ = nullptr;
  }

  [[nodiscard]] bool empty() const noexcept { return tiles_.empty(); }

 private:
  static constexpr int kTileShift = 6;
  static constexpr int kTileSize = 1 << kTileShift;
  static constexpr std::size_t kTilePixels = static_cast<std::size_t>(kTileSize) * kTileSize;

  struct Tile {
    std::array<T, kTilePixels> values{};
    std::bitset<kTilePixels> present{};
  };

  static std::size_t index_in_tile(std::int32_t x, std::int32_t y) noexcept {
    return static_cast<std::size_t>(y & (kTileSize - 1)) * kTileSize + static_cast<std::size_t>(x & (kTileSize - 1));
  }

  // Arithmetic shifts floor negative coordinates, so strokes that wander off
  // the canvas get their own tiles.
  static std::uint64_t tile_key(std::int32_t x, std::int32_t y) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y >> kTileShift)) << 32U) |
           static_cast<std::uint32_t>(x >> kTileShift);
  }

  Tile* tile_at(std::int32_t x, std::int32_t y, bool create) {
    const auto key = tile_key(x, y);
    if (cached_tile_ != nullptr && cached_key_ == key) {
      return cached_tile_;
    }
    auto found = tiles_.find(key);
    if (found == tiles_.end()) {
      if (!create) {
        return nullptr;
      }
      found = tiles_.emplace(key, std::make_unique<Tile>()).first;
    }
    cached_key_ = key;
    cached_tile_ = found->second.get();
    return cached_tile_;
  }

  std::unordered_map<std::uint64_t, std::unique_ptr<Tile>> tiles_;
  std::uint64_t cached_key_{0};
  Tile* cached_tile_{nullptr};
};

}  // namespace patchy::ui
