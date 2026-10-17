#pragma once

// Private interfaces between Affinity layer builders. No other importer includes
// this header. Helpers have one definition; parsing and rendering bodies stay in
// their owning translation units.
#include "core/document.hpp"
#include "core/vector_shape.hpp"
#include "formats/af_raster_internal.hpp"

#include <array>

namespace patchy::af::detail {

struct LayerBuildContext {
  std::span<const std::uint8_t> bytes;
  const Container& container;
  Document& document;
  std::vector<std::string>& notices;
  int embed_depth{0};
  int layer_count{0};
  // The doc.dat grammar version: the 1.x era writes 3..9, the 2.x/3.x
  // generation 20+. Embedded-document anchoring differs between them.
  std::uint32_t document_version{0};
  // The ancestor transform in wire order [a, b, tx, c, d, ty]: children of a
  // transformed container (artboards, vector shapes, groups) live in their
  // parent's LOCAL space, so the parent affine composes onto every descendant
  // (ns-logo's artboard grid and arkanis-discord's scaled template rects are
  // the wild pins). Raster-base clipped children keep uncomposed coordinates
  // (the corpus places them in spread space).
  std::array<double, 6> transform{1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
  static constexpr int kMaxLayers = 20000;   // runaway guard
  static constexpr int kMaxEmbedDepth = 3;   // embedded-document recursion cap
};

[[nodiscard]] std::array<double, 6> compose_transforms(const std::array<double, 6>& parent,
                                                       const std::array<double, 6>& child);

[[nodiscard]] std::vector<double> node_xfrm(const LayerBuildContext& ctx, const af::AfClass& node);

[[nodiscard]] const std::vector<std::shared_ptr<af::AfClass>>* class_list(const af::AfClass& node,
                                                                          std::uint32_t tag);

void apply_mask_children(LayerBuildContext& ctx, const af::AfClass& node, Layer& layer,
                         const std::string& name);

void apply_common(LayerBuildContext& ctx, const af::AfClass& node, Layer& layer,
                  const std::string& name);

[[nodiscard]] std::optional<std::array<float, 4>> read_rgba_color(const af::AfClass* color_class);

[[nodiscard]] std::int64_t enum_field_id(const af::AfClass& cls, std::uint32_t tag,
                                         std::int64_t fallback);

[[nodiscard]] std::optional<LayerStyleGradient> style_gradient_from_fill(const af::AfClass* descriptor);

[[nodiscard]] std::optional<Layer> build_text_layer(LayerBuildContext& ctx, const af::AfClass& node,
                                                    const std::string& display);

[[nodiscard]] std::optional<Layer> build_vector_layer(LayerBuildContext& ctx, const af::AfClass& node,
                                                      const std::string& display);

[[nodiscard]] std::optional<Layer> build_shape_layer(LayerBuildContext& ctx,
                                                     const af::AfClass& node,
                                                     const std::string& display,
                                                     std::string* why);

void append_vector_mask_paths(const af::AfClass& node, const std::array<double, 6>& base,
                              int depth, VectorPath& into, std::int32_t& group,
                              int& appended, int& unsupported);

[[nodiscard]] std::optional<VectorPath> crop_outline_path(LayerBuildContext& ctx,
                                                          const af::AfClass& node);

}  // namespace patchy::af::detail
