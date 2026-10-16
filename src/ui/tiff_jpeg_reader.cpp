#include "ui/tiff_jpeg_reader.hpp"

#include <QByteArray>
#include <QColorSpace>

#include <unordered_map>

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

namespace patchy::ui::tiff_jpeg {

namespace {

constexpr std::uint16_t kTagWidth = 256;
constexpr std::uint16_t kTagHeight = 257;
constexpr std::uint16_t kTagBitsPerSample = 258;
constexpr std::uint16_t kTagCompression = 259;
constexpr std::uint16_t kTagPhotometric = 262;
constexpr std::uint16_t kTagStripOffsets = 273;
constexpr std::uint16_t kTagRowsPerStrip = 278;
constexpr std::uint16_t kTagStripByteCounts = 279;
constexpr std::uint16_t kTagXResolution = 282;
constexpr std::uint16_t kTagYResolution = 283;
constexpr std::uint16_t kTagPlanarConfig = 284;
constexpr std::uint16_t kTagResolutionUnit = 296;
constexpr std::uint16_t kTagTileWidth = 322;
constexpr std::uint16_t kTagTileLength = 323;
constexpr std::uint16_t kTagTileOffsets = 324;
constexpr std::uint16_t kTagTileByteCounts = 325;
constexpr std::uint16_t kTagJpegTables = 347;
constexpr std::uint16_t kTagIccProfile = 34675;

struct Entry {
  std::uint16_t type{0};
  std::uint64_t count{0};
  std::size_t data_at{0};  // where the values start (inline or behind the offset)
};

class Directory {
public:
  bool parse(std::span<const std::uint8_t> bytes) {
    bytes_ = bytes;
    if (bytes.size() < 8) {
      return false;
    }
    little_endian_ = bytes[0] == 'I';
    if (!(bytes[0] == 'I' && bytes[1] == 'I') && !(bytes[0] == 'M' && bytes[1] == 'M')) {
      return false;
    }
    const auto magic = uint_at(2, 2);
    big_ = magic == 43;
    if (magic != 42 && !big_) {
      return false;
    }
    const std::size_t offset_width = big_ ? 8 : 4;
    const std::size_t count_width = big_ ? 8 : 2;
    const std::size_t entry_size = big_ ? 20 : 12;
    const auto ifd = static_cast<std::size_t>(uint_at(big_ ? 8 : 4, offset_width));
    if (ifd + count_width > bytes.size()) {
      return false;
    }
    const auto entries = uint_at(ifd, count_width);
    if (entries == 0 || entries > 65535 || ifd + count_width + entries * entry_size > bytes.size()) {
      return false;
    }
    for (std::uint64_t i = 0; i < entries; ++i) {
      const auto at = static_cast<std::size_t>(ifd + count_width + i * entry_size);
      Entry entry;
      const auto tag = static_cast<std::uint16_t>(uint_at(at, 2));
      entry.type = static_cast<std::uint16_t>(uint_at(at + 2, 2));
      entry.count = uint_at(at + 4, offset_width);
      const auto value_at = at + 4 + offset_width;
      const auto size = type_size(entry.type) * entry.count;
      entry.data_at = size > offset_width ? static_cast<std::size_t>(uint_at(value_at, offset_width)) : value_at;
      if (size == 0 || entry.data_at + size > bytes.size()) {
        continue;  // a damaged entry is skipped, not fatal
      }
      entries_[tag] = entry;
    }
    return true;
  }

  [[nodiscard]] std::optional<std::uint64_t> value(std::uint16_t tag, std::uint64_t index = 0) const {
    const auto it = entries_.find(tag);
    if (it == entries_.end() || index >= it->second.count) {
      return std::nullopt;
    }
    const auto& entry = it->second;
    const auto size = type_size(entry.type);
    if (size == 0 || entry.type == 5 || entry.type == 10) {
      return std::nullopt;
    }
    return uint_at(entry.data_at + static_cast<std::size_t>(index) * size, size);
  }

  [[nodiscard]] std::optional<double> rational(std::uint16_t tag) const {
    const auto it = entries_.find(tag);
    if (it == entries_.end() || it->second.type != 5) {
      return std::nullopt;
    }
    const auto numerator = uint_at(it->second.data_at, 4);
    const auto denominator = uint_at(it->second.data_at + 4, 4);
    if (denominator == 0) {
      return std::nullopt;
    }
    return static_cast<double>(numerator) / static_cast<double>(denominator);
  }

  [[nodiscard]] std::uint64_t count(std::uint16_t tag) const {
    const auto it = entries_.find(tag);
    return it == entries_.end() ? 0 : it->second.count;
  }

  [[nodiscard]] std::span<const std::uint8_t> raw(std::uint16_t tag) const {
    const auto it = entries_.find(tag);
    if (it == entries_.end()) {
      return {};
    }
    const auto size = static_cast<std::size_t>(type_size(it->second.type) * it->second.count);
    return bytes_.subspan(it->second.data_at, size);
  }

private:
  [[nodiscard]] static std::uint64_t type_size(std::uint16_t type) noexcept {
    switch (type) {
      case 1: case 2: case 6: case 7: return 1;
      case 3: case 8: return 2;
      case 4: case 9: case 11: case 13: return 4;
      case 5: case 10: case 12: case 16: case 17: case 18: return 8;
      default: return 0;
    }
  }

  [[nodiscard]] std::uint64_t uint_at(std::size_t offset, std::size_t width) const noexcept {
    if (offset + width > bytes_.size()) {
      return 0;
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
      const auto byte = static_cast<std::uint64_t>(bytes_[offset + (little_endian_ ? width - 1 - i : i)]);
      value = (value << 8) | byte;
    }
    return value;
  }

  std::span<const std::uint8_t> bytes_;
  bool little_endian_{true};
  bool big_{false};
  std::unordered_map<std::uint16_t, Entry> entries_;
};

// The JPEGTables payload without its SOI and EOI: what each abbreviated strip stream
// needs spliced in behind its own SOI.
QByteArray tables_body(std::span<const std::uint8_t> tables) {
  if (tables.size() < 4 || tables[0] != 0xff || tables[1] != 0xd8) {
    return {};
  }
  auto end = tables.size();
  if (tables[end - 2] == 0xff && tables[end - 1] == 0xd9) {
    end -= 2;
  }
  return QByteArray(reinterpret_cast<const char*>(tables.data() + 2), static_cast<qsizetype>(end - 2));
}

QImage decode_chunk(std::span<const std::uint8_t> chunk, const QByteArray& tables) {
  if (chunk.size() < 4) {
    return {};
  }
  QByteArray stream;
  if (chunk[0] == 0xff && chunk[1] == 0xd8 && !tables.isEmpty()) {
    stream.reserve(static_cast<qsizetype>(chunk.size()) + tables.size());
    stream.append("\xff\xd8", 2);
    stream.append(tables);
    stream.append(reinterpret_cast<const char*>(chunk.data() + 2), static_cast<qsizetype>(chunk.size() - 2));
  } else {
    stream = QByteArray(reinterpret_cast<const char*>(chunk.data()), static_cast<qsizetype>(chunk.size()));
  }
  return QImage::fromData(stream, "JPEG");
}

}  // namespace

bool is_jpeg_compressed_tiff(std::span<const std::uint8_t> bytes) noexcept {
  Directory directory;
  if (!directory.parse(bytes)) {
    return false;
  }
  return directory.value(kTagCompression).value_or(1) == 7;
}

Result read_jpeg_compressed_tiff(std::span<const std::uint8_t> bytes) {
  Result result;
  Directory directory;
  if (!directory.parse(bytes)) {
    result.error = QStringLiteral("not a TIFF");
    return result;
  }
  if (directory.value(kTagCompression).value_or(1) != 7) {
    result.error = QStringLiteral("not JPEG-compressed");
    return result;
  }
  const auto width = directory.value(kTagWidth).value_or(0);
  const auto height = directory.value(kTagHeight).value_or(0);
  if (width == 0 || height == 0 || width > 300000 || height > 300000) {
    result.error = QStringLiteral("bad image size");
    return result;
  }
  if (directory.value(kTagBitsPerSample).value_or(8) != 8) {
    result.error = QStringLiteral("only 8-bit samples");
    return result;
  }
  if (directory.value(kTagPlanarConfig).value_or(1) != 1) {
    result.error = QStringLiteral("separate planes");
    return result;
  }
  const bool tiled = directory.count(kTagTileOffsets) > 0;
  const auto offsets_tag = tiled ? kTagTileOffsets : kTagStripOffsets;
  const auto counts_tag = tiled ? kTagTileByteCounts : kTagStripByteCounts;
  const auto chunk_count = directory.count(offsets_tag);
  if (chunk_count == 0 || directory.count(counts_tag) != chunk_count) {
    result.error = QStringLiteral("no strips or tiles");
    return result;
  }
  std::uint64_t chunk_width = width;
  std::uint64_t chunk_height = directory.value(kTagRowsPerStrip).value_or(height);
  std::uint64_t across = 1;
  if (tiled) {
    chunk_width = directory.value(kTagTileWidth).value_or(0);
    chunk_height = directory.value(kTagTileLength).value_or(0);
    if (chunk_width == 0 || chunk_height == 0) {
      result.error = QStringLiteral("bad tile size");
      return result;
    }
    across = (width + chunk_width - 1) / chunk_width;
  }
  if (chunk_height == 0) {
    chunk_height = height;
  }
  const auto tables = tables_body(directory.raw(kTagJpegTables));

  QImage image;
  for (std::uint64_t i = 0; i < chunk_count; ++i) {
    const auto offset = directory.value(offsets_tag, i).value_or(0);
    const auto count = directory.value(counts_tag, i).value_or(0);
    if (offset == 0 || count == 0 || offset + count > bytes.size()) {
      result.error = QStringLiteral("strip or tile outside the file");
      return {};
    }
    const auto chunk = decode_chunk(bytes.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(count)), tables);
    if (chunk.isNull()) {
      result.error = QStringLiteral("a strip or tile did not decode");
      return result;
    }
    if (image.isNull()) {
      const auto format = chunk.format() == QImage::Format_Grayscale8   ? QImage::Format_Grayscale8
                          : chunk.format() == QImage::Format_CMYK8888  ? QImage::Format_CMYK8888
                                                                       : QImage::Format_RGB32;
      image = QImage(static_cast<int>(width), static_cast<int>(height), format);
      if (image.isNull()) {
        result.error = QStringLiteral("could not allocate the image");
        return result;
      }
      image.fill(format == QImage::Format_CMYK8888 ? 0U : 0xffffffffU);
    }
    const auto part = chunk.convertToFormat(image.format());
    const auto x0 = static_cast<int>((i % across) * chunk_width);
    const auto y0 = static_cast<int>((i / across) * chunk_height);
    const auto copy_w = std::min<int>(part.width(), image.width() - x0);
    const auto copy_h = std::min<int>(part.height(), image.height() - y0);
    if (copy_w <= 0 || copy_h <= 0) {
      continue;
    }
    const auto bpp = image.depth() / 8;
    for (int y = 0; y < copy_h; ++y) {
      std::memcpy(image.scanLine(y0 + y) + static_cast<std::size_t>(x0) * bpp, part.constScanLine(y),
                  static_cast<std::size_t>(copy_w) * bpp);
    }
  }

  // Photometric 5 (separated) data that libjpeg handed over as RGB cannot be fixed here;
  // the common Adobe-marker case arrives as CMYK8888 already.
  if (directory.value(kTagPhotometric).value_or(2) == 5 && image.format() != QImage::Format_CMYK8888) {
    result.error = QStringLiteral("CMYK strips without an Adobe marker");
    return result;
  }
  const auto icc = directory.raw(kTagIccProfile);
  if (!icc.empty()) {
    const auto space = QColorSpace::fromIccProfile(
        QByteArray(reinterpret_cast<const char*>(icc.data()), static_cast<qsizetype>(icc.size())));
    if (space.isValid()) {
      image.setColorSpace(space);
    }
  }
  const auto unit = directory.value(kTagResolutionUnit).value_or(2);
  const auto x_res = directory.rational(kTagXResolution);
  const auto y_res = directory.rational(kTagYResolution);
  if (x_res.has_value() && y_res.has_value() && *x_res > 0.0 && *y_res > 0.0 && (unit == 2 || unit == 3)) {
    const double per_meter = unit == 2 ? 39.3700787 : 100.0;
    image.setDotsPerMeterX(static_cast<int>(*x_res * per_meter + 0.5));
    image.setDotsPerMeterY(static_cast<int>(*y_res * per_meter + 0.5));
  }
  result.image = std::move(image);
  return result;
}

}  // namespace patchy::ui::tiff_jpeg
