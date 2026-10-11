#pragma once

#include "formats/metadata/metadata_edit.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// Splices File Info metadata into an encoded image (docs/file-info.md). The encoders
// (Qt's JPEG/PNG/TIFF/WebP writers, Patchy's GIF writer) produce the pixels; this adds
// EXIF, XMP, IPTC, the ICC profile and text in each format's own containers, after
// removing whatever the encoder wrote in the same containers so nothing is doubled.
// Qt-free. An empty SaveMetadata returns the input unchanged, byte for byte.
namespace patchy::metadata {

// `format` is the lowercase extension family: "jpeg"/"jpg", "png", "tif"/"tiff",
// "webp", "gif". Unknown formats and unparseable input come back unchanged.
[[nodiscard]] std::vector<std::uint8_t> inject_metadata(std::vector<std::uint8_t> encoded, std::string_view format,
                                                        const SaveMetadata& metadata);

}  // namespace patchy::metadata
