#pragma once

#include "render/layer_render_types.hpp"

namespace patchy {
struct PatternStore;

namespace render_detail {

// Non-template operations shared with UI tools. Per-pixel compositor templates
// remain private to the translation units that implement render targets.
std::vector<float> build_mask_coverage_plane(const Layer& layer, Rect draw_rect,
                                             std::optional<Rect> mask_bounds_override);

// RGB8 child silhouette over the complete requested bounds. This preserves the
// current thread's raster-view context and uses no persistent style-mask cache.
PixelBuffer group_silhouette_rgb8(const Layer& layer, Rect bounds, const PatternStore* patterns);

}  // namespace render_detail
}  // namespace patchy
