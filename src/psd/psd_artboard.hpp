#pragma once

#include "core/document.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Photoshop's artboard records (docs/artboards.md, layouts observed from Photoshop 2026's
// own files, test-fixtures/psd/photoshop-artboard-*.psd):
// - 'artb' on an artboard's folder record: version 16 + descriptor class "artboard" with
//   artboardRect (classFloatRect Top/Left/Btom/Rght doubles, document pixels),
//   guideIndeces (list), artboardPresetName (text), 'Clr ' (RGBC doubles) and
//   artboardBackgroundType (1 white, 2 black, 3 transparent, 4 other). Older files name
//   the same record 'abdd' or 'artd', and some leave keys out.
// - 'artd' among the global blocks: version 16 + descriptor 'null' with the count, the
//   auto-expand offset and origin points, the behavior flags, and the default background.
namespace patchy::psd {

// True for the per-layer keys that carry an artboard record.
[[nodiscard]] bool is_artboard_block_key(std::string_view key) noexcept;

[[nodiscard]] std::optional<LayerArtboard> parse_artboard_block(std::span<const std::uint8_t> payload);
// The record exactly as Photoshop writes it (key order, id forms, padded to 4 bytes): a
// Photoshop block parses and regenerates byte for byte.
[[nodiscard]] std::vector<std::uint8_t> artboard_block_payload(const LayerArtboard& artboard);

struct DocumentArtboardBlock {
  DocumentArtboardSettings settings;
  std::int32_t count{0};
};
[[nodiscard]] std::optional<DocumentArtboardBlock> parse_document_artboard_block(std::span<const std::uint8_t> payload);
[[nodiscard]] std::vector<std::uint8_t> document_artboard_block_payload(const DocumentArtboardSettings& settings,
                                                                        std::int32_t count);

}  // namespace patchy::psd
