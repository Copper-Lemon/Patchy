#include "ui/apng_reader.hpp"

#include <QByteArray>
#include <QPainter>

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

namespace patchy::apng {

namespace {

constexpr std::array<std::uint8_t, 8> kPngSignature{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
constexpr std::uint32_t kMaxFrames = 100000;

struct Chunk {
  std::uint32_t type{};
  std::span<const std::uint8_t> data;
};

constexpr std::uint32_t chunk_type(const char (&name)[5]) {
  return (static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[0])) << 24) |
         (static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[1])) << 16) |
         (static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[2])) << 8) |
         static_cast<std::uint32_t>(static_cast<std::uint8_t>(name[3]));
}

constexpr auto kIHDR = chunk_type("IHDR");
constexpr auto kPLTE = chunk_type("PLTE");
constexpr auto kIDAT = chunk_type("IDAT");
constexpr auto kIEND = chunk_type("IEND");
constexpr auto kacTL = chunk_type("acTL");
constexpr auto kfcTL = chunk_type("fcTL");
constexpr auto kfdAT = chunk_type("fdAT");
constexpr auto ktRNS = chunk_type("tRNS");
constexpr auto kgAMA = chunk_type("gAMA");
constexpr auto kcHRM = chunk_type("cHRM");
constexpr auto ksRGB = chunk_type("sRGB");
constexpr auto kiCCP = chunk_type("iCCP");
constexpr auto ksBIT = chunk_type("sBIT");

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return (static_cast<std::uint32_t>(bytes[offset]) << 24) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) | static_cast<std::uint32_t>(bytes[offset + 3]);
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

bool has_png_signature(std::span<const std::uint8_t> bytes) noexcept {
  return bytes.size() >= kPngSignature.size() &&
         std::memcmp(bytes.data(), kPngSignature.data(), kPngSignature.size()) == 0;
}

// Walks the chunk stream; stops quietly at a truncated chunk (the frames that
// decoded are kept, like the GIF import does with a damaged tail).
template <typename Visit>
void for_each_chunk(std::span<const std::uint8_t> bytes, Visit&& visit) {
  std::size_t offset = kPngSignature.size();
  while (offset + 12 <= bytes.size()) {
    const auto length = read_u32(bytes, offset);
    if (length > bytes.size() - offset - 12) {
      return;
    }
    Chunk chunk;
    chunk.type = read_u32(bytes, offset + 4);
    chunk.data = bytes.subspan(offset + 8, length);
    if (!visit(chunk)) {
      return;
    }
    offset += 12 + static_cast<std::size_t>(length);
  }
}

const std::array<std::uint32_t, 256>& crc_table() {
  static const auto table = [] {
    std::array<std::uint32_t, 256> values{};
    for (std::uint32_t n = 0; n < 256; ++n) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1U) != 0 ? 0xedb88320U ^ (c >> 1) : c >> 1;
      }
      values[n] = c;
    }
    return values;
  }();
  return table;
}

std::uint32_t crc32(std::span<const std::uint8_t> bytes, std::uint32_t crc = 0) {
  crc ^= 0xffffffffU;
  for (const auto byte : bytes) {
    crc = crc_table()[(crc ^ byte) & 0xffU] ^ (crc >> 8);
  }
  return crc ^ 0xffffffffU;
}

void append_u32(QByteArray& out, std::uint32_t value) {
  const char bytes[4] = {static_cast<char>(value >> 24), static_cast<char>(value >> 16), static_cast<char>(value >> 8),
                         static_cast<char>(value)};
  out.append(bytes, 4);
}

void append_chunk(QByteArray& out, std::uint32_t type, std::span<const std::uint8_t> data) {
  append_u32(out, static_cast<std::uint32_t>(data.size()));
  const std::uint8_t type_bytes[4] = {static_cast<std::uint8_t>(type >> 24), static_cast<std::uint8_t>(type >> 16),
                                      static_cast<std::uint8_t>(type >> 8), static_cast<std::uint8_t>(type)};
  out.append(reinterpret_cast<const char*>(type_bytes), 4);
  out.append(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
  append_u32(out, crc32(data, crc32(std::span<const std::uint8_t>(type_bytes, 4))));
}

struct FrameControl {
  std::uint32_t sequence{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t x{};
  std::uint32_t y{};
  std::uint32_t delay_ms{};
  std::uint8_t dispose{};  // 0 none, 1 background, 2 previous
  std::uint8_t blend{};    // 0 source, 1 over
};

FrameControl parse_frame_control(std::span<const std::uint8_t> data) {
  if (data.size() < 26) {
    throw std::runtime_error("fcTL chunk is too short");
  }
  FrameControl control;
  control.sequence = read_u32(data, 0);
  control.width = read_u32(data, 4);
  control.height = read_u32(data, 8);
  control.x = read_u32(data, 12);
  control.y = read_u32(data, 16);
  const auto delay_num = read_u16(data, 20);
  auto delay_den = read_u16(data, 22);
  if (delay_den == 0) {
    delay_den = 100;  // the specification's substitute
  }
  control.delay_ms = static_cast<std::uint32_t>((static_cast<std::uint64_t>(delay_num) * 1000U + delay_den / 2) /
                                                delay_den);
  control.dispose = data[24];
  control.blend = data[25];
  return control;
}

// One frame's standalone PNG: the file's IHDR with the frame's size, the
// palette and color chunks that precede the first image data, the frame's
// data as IDAT, and IEND.
QImage decode_frame(std::span<const std::uint8_t> ihdr, const std::vector<Chunk>& shared, const FrameControl& control,
                    const std::vector<std::span<const std::uint8_t>>& data_chunks) {
  QByteArray png;
  png.append(reinterpret_cast<const char*>(kPngSignature.data()), static_cast<qsizetype>(kPngSignature.size()));
  std::array<std::uint8_t, 13> header{};
  std::memcpy(header.data(), ihdr.data(), header.size());
  header[0] = static_cast<std::uint8_t>(control.width >> 24);
  header[1] = static_cast<std::uint8_t>(control.width >> 16);
  header[2] = static_cast<std::uint8_t>(control.width >> 8);
  header[3] = static_cast<std::uint8_t>(control.width);
  header[4] = static_cast<std::uint8_t>(control.height >> 24);
  header[5] = static_cast<std::uint8_t>(control.height >> 16);
  header[6] = static_cast<std::uint8_t>(control.height >> 8);
  header[7] = static_cast<std::uint8_t>(control.height);
  append_chunk(png, kIHDR, header);
  for (const auto& chunk : shared) {
    append_chunk(png, chunk.type, chunk.data);
  }
  for (const auto& data : data_chunks) {
    append_chunk(png, kIDAT, data);
  }
  append_chunk(png, kIEND, {});
  QImage image;
  if (!image.loadFromData(png, "PNG")) {
    throw std::runtime_error("frame " + std::to_string(control.sequence) + " does not decode");
  }
  return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

}  // namespace

bool is_animated_png(std::span<const std::uint8_t> bytes) noexcept {
  if (!has_png_signature(bytes)) {
    return false;
  }
  bool animated = false;
  for_each_chunk(bytes, [&](const Chunk& chunk) {
    if (chunk.type == kacTL && chunk.data.size() >= 8) {
      animated = read_u32(chunk.data, 0) > 1;
      return false;
    }
    return chunk.type != kIDAT && chunk.type != kIEND;  // acTL must precede the image data
  });
  return animated;
}

Animation read_animated_png(std::span<const std::uint8_t> bytes) {
  if (!has_png_signature(bytes)) {
    throw std::runtime_error("not a PNG file");
  }
  Animation animation;
  std::span<const std::uint8_t> ihdr;
  std::vector<Chunk> shared;
  bool seen_image_data = false;
  std::uint32_t declared_frames = 0;
  std::uint32_t expected_sequence = 0;

  struct PendingFrame {
    FrameControl control;
    std::vector<std::span<const std::uint8_t>> data;
  };
  std::vector<PendingFrame> pending;
  bool default_image_in_animation = false;

  for_each_chunk(bytes, [&](const Chunk& chunk) {
    switch (chunk.type) {
      case kIHDR:
        if (chunk.data.size() < 13) {
          throw std::runtime_error("IHDR chunk is too short");
        }
        ihdr = chunk.data;
        animation.width = static_cast<int>(read_u32(chunk.data, 0));
        animation.height = static_cast<int>(read_u32(chunk.data, 4));
        return true;
      case kacTL:
        if (chunk.data.size() >= 8) {
          declared_frames = read_u32(chunk.data, 0);
          animation.play_count = read_u32(chunk.data, 4);
        }
        return true;
      case kPLTE:
      case ktRNS:
      case kgAMA:
      case kcHRM:
      case ksRGB:
      case kiCCP:
      case ksBIT:
        if (!seen_image_data) {
          shared.push_back(chunk);
        }
        return true;
      case kfcTL: {
        auto control = parse_frame_control(chunk.data);
        if (control.sequence != expected_sequence) {
          throw std::runtime_error("frame control sequence numbers are out of order");
        }
        ++expected_sequence;
        if (control.width == 0 || control.height == 0 ||
            static_cast<std::uint64_t>(control.x) + control.width > static_cast<std::uint64_t>(animation.width) ||
            static_cast<std::uint64_t>(control.y) + control.height > static_cast<std::uint64_t>(animation.height)) {
          throw std::runtime_error("frame " + std::to_string(control.sequence) + " lies outside the canvas");
        }
        if (pending.size() >= kMaxFrames) {
          throw std::runtime_error("too many frames");
        }
        if (!seen_image_data) {
          default_image_in_animation = true;
        }
        pending.push_back({control, {}});
        return true;
      }
      case kIDAT:
        seen_image_data = true;
        if (default_image_in_animation && !pending.empty()) {
          pending.front().data.push_back(chunk.data);
        }
        return true;
      case kfdAT: {
        if (chunk.data.size() < 4) {
          throw std::runtime_error("fdAT chunk is too short");
        }
        if (read_u32(chunk.data, 0) != expected_sequence) {
          throw std::runtime_error("frame data sequence numbers are out of order");
        }
        ++expected_sequence;
        if (pending.empty()) {
          throw std::runtime_error("frame data before any frame control");
        }
        pending.back().data.push_back(chunk.data.subspan(4));
        return true;
      }
      case kIEND:
        return false;
      default:
        return true;
    }
  });
  if (ihdr.empty()) {
    throw std::runtime_error("no IHDR chunk");
  }
  if (declared_frames < 2 || pending.size() < 2) {
    throw std::runtime_error("not an animation");
  }
  if (animation.width <= 0 || animation.height <= 0) {
    throw std::runtime_error("empty canvas");
  }

  // Composite in sequence. The first frame's "previous" disposal counts as
  // "background", per the specification.
  QImage canvas(animation.width, animation.height, QImage::Format_ARGB32_Premultiplied);
  canvas.fill(Qt::transparent);
  for (std::size_t index = 0; index < pending.size(); ++index) {
    auto& frame = pending[index];
    if (frame.data.empty()) {
      throw std::runtime_error("frame " + std::to_string(frame.control.sequence) + " has no image data");
    }
    const auto decoded = decode_frame(ihdr, shared, frame.control, frame.data);
    const QRect region(static_cast<int>(frame.control.x), static_cast<int>(frame.control.y),
                       static_cast<int>(frame.control.width), static_cast<int>(frame.control.height));
    auto dispose = frame.control.dispose;
    if (index == 0 && dispose == 2) {
      dispose = 1;
    }
    const QImage before_frame = dispose == 2 ? canvas.copy() : QImage();
    {
      QPainter painter(&canvas);
      painter.setCompositionMode(frame.control.blend == 0 ? QPainter::CompositionMode_Source
                                                           : QPainter::CompositionMode_SourceOver);
      painter.drawImage(region.topLeft(), decoded);
    }
    animation.frames.push_back({canvas.copy(), frame.control.delay_ms});
    if (dispose == 1) {
      QPainter painter(&canvas);
      painter.setCompositionMode(QPainter::CompositionMode_Source);
      painter.fillRect(region, Qt::transparent);
    } else if (dispose == 2) {
      canvas = before_frame;
    }
  }
  return animation;
}

}  // namespace patchy::apng
