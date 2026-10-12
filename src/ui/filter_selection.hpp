#pragma once

#include "core/layer.hpp"

#include <QRegion>

#include <utility>

namespace patchy::ui {

// The selection a destructive filter or adjustment runs under. `region` bounds the work
// and everything outside it is restored; a soft selection (a feather, Object Selection's
// anti-aliased edge) also carries its coverage, and each result pixel is blended with the
// original by that coverage, as in Photoshop. Converts to and from QRegion, so hard
// selections and the region-only helpers are unchanged. A filter preview proxy carries
// one scaled to its own pixels (filter_preview_proxy.hpp).
struct FilterSelection {
  FilterSelection() = default;
  FilterSelection(QRegion selection_region)  // NOLINT(google-explicit-constructor)
      : region(std::move(selection_region)) {}
  operator const QRegion&() const noexcept { return region; }  // NOLINT(google-explicit-constructor)
  [[nodiscard]] bool isEmpty() const noexcept { return region.isEmpty(); }

  QRegion region;
  Rect coverage_bounds{};  // document coordinates of `coverage`
  PixelBuffer coverage;    // gray8, 255 = fully selected; empty for a hard selection
};

}  // namespace patchy::ui
