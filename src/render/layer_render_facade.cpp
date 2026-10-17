#include "render/layer_render_facade.hpp"

#include "render/layer_compositor.hpp"

namespace patchy::render_detail {

PixelBuffer group_silhouette_rgb8(const Layer& layer, Rect bounds, const PatternStore* patterns) {
  return group_silhouette_for_render(layer, bounds, nullptr, false, nullptr, patterns);
}

}  // namespace patchy::render_detail
