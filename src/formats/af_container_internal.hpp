#pragma once

// Private Affinity container interface. Codec headers stay in af_container.cpp.
#include "core/pixel_buffer.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace patchy::af::detail {

constexpr std::uint32_t kMagic = 0x414BFF00U;

// The newest container version this reader has been verified against. Newer
// files still get a best-effort preview (the walk is version-stable so far);
// a notice records the mismatch.
constexpr std::uint16_t kNewestVerifiedContainerVersion = 12;

constexpr std::uint64_t kMaxStreamBytes = 512ULL * 1024ULL * 1024ULL;

struct StreamRecord {
  std::uint64_t data_offset{0};
  std::uint64_t size{0};
  std::uint64_t compressed_size{0};
  std::uint32_t crc32{0};
  std::uint8_t compression{0};
};

struct Container {
  std::uint16_t version{0};
  std::uint16_t flags{0};
  std::uint32_t class_tag{0};
  std::uint64_t thumbnail_offset{0};
  std::uint32_t protocol{0};
  // Head revision per stream name (the newest table entry wins, as in the app).
  std::unordered_map<std::string, StreamRecord> streams;
};

[[nodiscard]] Container parse_container(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::vector<std::uint8_t> extract_stream(std::span<const std::uint8_t> bytes,
                                                       const StreamRecord& record,
                                                       const std::string& name,
                                                       std::vector<std::string>* notices);

[[nodiscard]] PixelBuffer extract_preview(std::span<const std::uint8_t> bytes,
                                          const Container& container);

}  // namespace patchy::af::detail
