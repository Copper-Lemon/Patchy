#pragma once

// JPEG-compressed TIFF (compression 7) through Qt's JPEG decoder. Qt's bundled libtiff is
// built without a JPEG codec, so QImageReader rejects every such file (scanners and
// cameras write them; GitHub issue 117). This reader walks the first directory, glues the
// shared JPEGTables in front of each strip or tile's abbreviated stream, decodes each with
// QImage::fromData and pastes it into one image. 8-bit samples, chunky planar layout;
// grayscale, RGB, YCbCr and CMYK photometrics (CMYK comes back as Format_CMYK8888 with
// the file's ICC profile attached, like Qt's own TIFF reader, so the CMYK import path
// applies). Density tags set the image's dots per meter.

#include <QImage>
#include <QString>

#include <cstdint>
#include <span>

namespace patchy::ui::tiff_jpeg {

struct Result {
  QImage image;   // null when the file could not be decoded
  QString error;  // why, in English (the caller localizes or keeps the decoder's text)
};

// True when the bytes are a TIFF whose first directory says compression 7.
[[nodiscard]] bool is_jpeg_compressed_tiff(std::span<const std::uint8_t> bytes) noexcept;

[[nodiscard]] Result read_jpeg_compressed_tiff(std::span<const std::uint8_t> bytes);

}  // namespace patchy::ui::tiff_jpeg
