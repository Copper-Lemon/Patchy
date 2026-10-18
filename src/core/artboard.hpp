#pragma once

#include "core/document.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Artboards (docs/artboards.md): Photoshop's canvases inside a document. An artboard is a
// TOP-LEVEL group carrying a LayerArtboard (core/layer.hpp); everything here is Qt-free.
//
// Ground truth (Photoshop 2026, test-fixtures/psd/photoshop-artboard-*.psd):
// - an artboard's children clip to its rect, and its background paints under them inside
//   the group, so the group's opacity and blend mode apply to both;
// - a new artboard is Normal (isolated), not Pass Through like a plain group;
// - layers outside every artboard render normally, and the composite outside every
//   artboard and every loose layer is transparent;
// - artboards never nest and never sit inside a plain group.

namespace patchy {

[[nodiscard]] bool layer_is_artboard(const Layer& layer) noexcept;
[[nodiscard]] bool document_has_artboards(const Document& document) noexcept;

// Top-level artboards, TOP of the stack first: the Layers panel's order and the page order
// of every per-artboard export.
[[nodiscard]] std::vector<const Layer*> document_artboards(const Document& document);
[[nodiscard]] std::vector<Layer*> document_artboards(Document& document);

// The topmost artboard whose rect contains the point, or null.
[[nodiscard]] const Layer* artboard_at(const Document& document, std::int32_t x, std::int32_t y);

// The artboard a layer lives in (the layer itself when it is one), or null.
[[nodiscard]] const Layer* artboard_owning_layer(const Document& document, LayerId id);

// The top-level layer whose tree holds `id` (`id` itself when it is top level), or
// nullopt. An artboard inserted "above" a nested layer goes above this one instead, so
// artboards stay top level (paste, cross-document copies).
[[nodiscard]] std::optional<LayerId> top_level_ancestor_id(const Document& document, LayerId id);

// The union of every artboard's rect, or nullopt without artboards.
[[nodiscard]] std::optional<Rect> artboard_union_bounds(const Document& document);

// "Artboard N", one past the highest N any top-level layer already uses (Photoshop's
// naming for New Artboard).
[[nodiscard]] std::string next_artboard_name(const Document& document);

// A group configured the way Photoshop makes an artboard: Normal blend, the rect, the
// background. The caller allocates the id from the target document.
[[nodiscard]] Layer make_artboard_group(LayerId id, std::string name, Rect rect,
                                        ArtboardBackground background = ArtboardBackground::White,
                                        RgbColor color = RgbColor{255, 255, 255});

// The colour an artboard's background paints (straight RGBA), or nullopt for Transparent.
[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> artboard_background_rgba(const LayerArtboard& artboard) noexcept;

// The canvas a document with artboards needs: the union of every artboard's rect and every
// visible loose top-level layer's render bounds (Photoshop's shrinkwrap). nullopt without
// artboards.
[[nodiscard]] std::optional<Rect> artboard_document_extent(const Document& document);

// Grows or shrinks the canvas to artboard_document_extent, moving content so the
// top-left extent lands at (0, 0). Returns the shift applied to document coordinates
// (add it to a point to follow the content), or nullopt when nothing changed.
std::optional<std::array<std::int32_t, 2>> fit_canvas_to_artboards(Document& document);

// Grows the canvas (never shrinks it) until every artboard fits, the live counterpart of
// fit_canvas_to_artboards that the Artboard tool runs after each edit, so the canvas does
// not jump while one is dragged around. Same return value. Does nothing when the grown
// canvas would pass kMaxArtboardCanvasSide (callers check artboard_canvas_within_limit
// first and refuse the edit).
std::optional<std::array<std::int32_t, 2>> grow_canvas_to_artboards(Document& document);

// The largest canvas side artboard edits may grow to (the script resizeCanvas limit).
inline constexpr std::int32_t kMaxArtboardCanvasSide = 30000;

// The canvas grow_canvas_to_artboards would make (the canvas when nothing grows), in
// 64-bit so far-off rects cannot overflow, and whether both its sides are within
// kMaxArtboardCanvasSide.
[[nodiscard]] bool artboard_canvas_within_limit(const Document& document);
[[nodiscard]] bool artboard_canvas_would_grow(const Document& document);

// The top-level index an artboard goes to when it is made: above every other top-level
// layer, like Photoshop's New Artboard.
[[nodiscard]] std::size_t new_artboard_insert_index(const Document& document) noexcept;

// Where a new artboard of `size` lands beside the others: right of the rightmost, 100 px
// apart, top-aligned with it (Photoshop's add-button and Duplicate placement). With no
// artboards it is the canvas origin.
[[nodiscard]] Rect artboard_rect_beside(const Document& document, std::int32_t width, std::int32_t height);

// The rect an artboard of `size` gets when added next to `anchor` on `side`
// (0 right, 1 bottom, 2 left, 3 top), 100 px apart.
[[nodiscard]] Rect artboard_rect_next_to(Rect anchor, int side, std::int32_t width, std::int32_t height) noexcept;

inline constexpr std::int32_t kArtboardSpacing = 100;

// Artboard from Layers: moves the listed layers (any order; their stacking order is kept)
// onto a new artboard named `name` ("Artboard N" when empty) at the top of the stack,
// sized to their render bounds, or the canvas when nothing of them renders. Returns the
// new artboard's id, or nullopt (the document untouched) when the list is empty or a
// listed layer is missing, is an artboard, lives on one, or sits inside another listed
// layer.
// Auto-nesting (docs/artboards.md): a layer moved by the Move tool belongs to the
// artboard under its center afterwards, or to the top level when it is over none. The
// Move tool previews exactly what the commit applies, so both go through these.
struct ArtboardNestMove {
  LayerId id{};
  std::optional<LayerId> target;  // nullopt: the top level
  friend bool operator==(const ArtboardNestMove&, const ArtboardNestMove&) = default;
};

// The layers a move re-nests: the roots of `selected_ids` (a moved folder moves as one)
// that hold one of `moved_leaf_ids`, else the moved leaves' own roots; never an artboard.
[[nodiscard]] std::vector<LayerId> artboard_nest_roots(const Document& document,
                                                       const std::vector<LayerId>& selected_ids,
                                                       const std::vector<LayerId>& moved_leaf_ids);

// For `root_ids` shifted by (dx, dy) from where they are now: the ones whose artboard
// changes, with the artboard they join (or the top level).
[[nodiscard]] std::vector<ArtboardNestMove> artboard_nest_moves(const Document& document,
                                                                const std::vector<LayerId>& root_ids,
                                                                std::int32_t dx, std::int32_t dy);

// Applies them: a layer joining an artboard goes on top of its children; one leaving
// for the top level goes right above the artboard it left. True when anything moved.
bool apply_artboard_nest_moves(Document& document, const std::vector<ArtboardNestMove>& moves);

// True when an artboard composites like plain paper plus clip: Normal or Pass Through,
// full opacity and fill, no masks, no style, so a preview may draw its content clipped
// to its rect over the base frame (the Move tool's proxy).
[[nodiscard]] bool artboard_composites_plainly(const Layer& artboard) noexcept;

std::optional<LayerId> put_layers_on_new_artboard(Document& document, const std::vector<LayerId>& ids,
                                                  std::string name = {});

// Rewrites every artboard into the plain layers that composite the same way, for writers
// that have no artboards of their own (editable PDF, SVG): the group stays (isolated, its
// opacity and blend mode kept), gains a rectangular vector mask that clips it to the
// rect, and a non-Transparent background becomes a solid shape layer at its bottom
// named "<artboard> Background".
void convert_artboards_to_plain_groups(Document& document);

}  // namespace patchy
