#include "formats/af_import_internal.hpp"

#include "core/environment.hpp"
#include "core/layer_metadata.hpp"
#include "core/vector_raster.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace patchy::af::detail {
namespace {

// ---- Vector curve nodes (PCrv) -> Patchy shape layers ----

// A vector fill from an FDsc descriptor: FDeF is FilS (solid Colr), FilG
// (gradient - the same shape style_gradient_from_fill decodes, including the
// FDeX placement transform), or FilN (none). Patchy's VectorFill carries no
// alpha, so the color's alpha comes back separately.
struct AfVectorFill {
  VectorFill fill;
  float alpha{1.0F};
};

// A node's descriptor field is a single class in some documents and a
// one-element class list in others (BFFl/LIFl mirror LILn's list shape).
[[nodiscard]] const af::AfClass* first_class_of(const af::AfClass& node, std::uint32_t tag) {
  if (const af::AfClass* single = node.child_class(tag)) {
    return single;
  }
  if (const auto* list = class_list(node, tag); list != nullptr && !list->empty()) {
    return list->front().get();
  }
  return nullptr;
}

[[nodiscard]] std::optional<AfVectorFill> vector_fill_from_descriptor(const af::AfClass* descriptor) {
  if (descriptor == nullptr) {
    return std::nullopt;
  }
  const af::AfClass* fill = descriptor->child_class(af::tag4("FDeF"));
  if (fill == nullptr && (descriptor->type_tag == af::tag4("FilS") ||
                          descriptor->type_tag == af::tag4("FilN") ||
                          descriptor->type_tag == af::tag4("FilG"))) {
    // The oldest generation (doc versions 3..9) stores the Fill class
    // DIRECTLY on the field (vh-check's BFil/PFil) instead of wrapping it in
    // an FDsc; a direct FilG has no FDeX placement and stays undecoded.
    fill = descriptor;
  }
  if (fill == nullptr) {
    return std::nullopt;
  }
  AfVectorFill result;
  if (fill->type_tag == af::tag4("FilN")) {
    result.fill.kind = VectorFillKind::None;
    return result;
  }
  if (fill->type_tag == af::tag4("FilG")) {
    if (auto gradient = style_gradient_from_fill(descriptor)) {
      result.fill.kind = VectorFillKind::Gradient;
      result.fill.gradient = std::move(*gradient);
      return result;
    }
    return std::nullopt;
  }
  if (const auto stored = read_rgba_color(fill->child_class(af::tag4("Colr")))) {
    const auto to_channel = [](float value) {
      return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    };
    result.fill.kind = VectorFillKind::Solid;
    result.fill.color =
        RgbColor{to_channel((*stored)[0]), to_channel((*stored)[1]), to_channel((*stored)[2])};
    result.alpha = std::clamp((*stored)[3], 0.0F, 1.0F);
    return result;
  }
  return std::nullopt;
}

// The node's path: Crvs -> PCvD -> Data, an UNTAGGED inline class laid out as
// [u8 version][u32 subpath count] then per subpath [bool closed][point
// curve-array]. Each 18-byte point record is x f64 LE, y f64 LE, u16 flags:
// 0x0001 corner anchor, 0x0002 smooth anchor, 0x0100 = the previous anchor's
// control-out, 0x0200 = the NEXT anchor's control-in. Closed subpaths repeat
// the first anchor at the end (folded away below). All subpaths share shape
// group 0: Affinity fills a poly-curve even-odd (the two same-winding nested
// rectangles of the donut probe render a hole), which is exactly Patchy's
// within-group rule.
// Symbol instances (the 2.x generation's Designer symbols): a linked instance
// stores no geometry of its own; its SLnk -> ILSN -> ILOb ring lists every
// sibling instance of the same symbol member, and the defining sibling carries
// the real record (Crvs for curves, Shpe for parametric shapes). The tree
// parser resolves shared references to the defining AfClass, so one hop
// through the ring finds it. Pinned by the beat-bot wild file (two symbol
// raccoons; the second is all linked instances). Paint (BFFl/LILn/LIFl) and
// the transform stay per-instance on the node itself.
[[nodiscard]] const af::AfClass* symbol_sibling_with(const af::AfClass& node,
                                                     std::uint32_t field_tag) {
  const af::AfClass* link = node.child_class(af::tag4("SLnk"));
  if (link == nullptr) {
    return nullptr;
  }
  const auto* members = class_list(*link, af::tag4("ILOb"));
  if (members == nullptr) {
    return nullptr;
  }
  for (const auto& member : *members) {
    if (member != nullptr && member.get() != &node &&
        member->child_class(field_tag) != nullptr) {
      return member.get();
    }
  }
  return nullptr;
}

[[nodiscard]] std::optional<VectorPath> vector_path_from_node(const af::AfClass& node) {
  const af::AfClass* curves = node.child_class(af::tag4("Crvs"));
  if (curves == nullptr) {
    // Compound (Comp) nodes store their baked, already-booleaned result
    // poly-curve under a lowercase 'crvs' field with the same PCvD layout.
    curves = node.child_class(af::tag4("crvs"));
  }
  if (curves == nullptr) {
    if (const af::AfClass* sibling = symbol_sibling_with(node, af::tag4("Crvs"))) {
      curves = sibling->child_class(af::tag4("Crvs"));
    }
  }
  if (curves == nullptr) {
    return std::nullopt;
  }
  const af::AfClass* data = curves->child_class(af::tag4("Data"));
  if (data == nullptr) {
    return std::nullopt;
  }
  const auto read_f64 = [](const std::uint8_t* bytes) {
    std::uint64_t bits = 0;
    for (int i = 7; i >= 0; --i) {
      bits = (bits << 8U) | bytes[i];
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  };
  VectorPath path;
  bool closed = true;
  for (const auto& field : data->fields) {
    if (const auto* flag = std::get_if<bool>(&field.value)) {
      closed = *flag;
      continue;
    }
    const auto* records = std::get_if<af::AfCurveArray>(&field.value);
    if (records == nullptr || records->record_size < 18) {
      continue;
    }
    PathSubpath subpath;
    subpath.closed = closed;
    subpath.op = PathCombineOp::Add;
    subpath.shape_group = 0;
    bool pending_in = false;
    double in_x = 0.0;
    double in_y = 0.0;
    const std::size_t stride = records->record_size;
    for (std::size_t at = 0; at + stride <= records->bytes.size(); at += stride) {
      const std::uint8_t* bytes = records->bytes.data() + at;
      const double x = read_f64(bytes);
      const double y = read_f64(bytes + 8);
      if (!std::isfinite(x) || !std::isfinite(y)) {
        continue;
      }
      const std::uint16_t flags =
          static_cast<std::uint16_t>(bytes[16] | (static_cast<std::uint16_t>(bytes[17]) << 8U));
      if ((flags & 0x0003U) != 0) {
        PathAnchor anchor{x, y, x, y, x, y, (flags & 0x0002U) != 0};
        if (pending_in) {
          anchor.in_x = in_x;
          anchor.in_y = in_y;
          pending_in = false;
        }
        subpath.anchors.push_back(anchor);
      } else if ((flags & 0x0100U) != 0) {
        if (!subpath.anchors.empty()) {
          subpath.anchors.back().out_x = x;
          subpath.anchors.back().out_y = y;
        }
      } else if ((flags & 0x0200U) != 0) {
        pending_in = true;
        in_x = x;
        in_y = y;
      }
    }
    // The closing record repeats anchor 0; fold its incoming handle there.
    if (subpath.closed && subpath.anchors.size() >= 2) {
      const auto& first = subpath.anchors.front();
      const auto& last = subpath.anchors.back();
      if (std::abs(first.anchor_x - last.anchor_x) < 0.0001 &&
          std::abs(first.anchor_y - last.anchor_y) < 0.0001) {
        subpath.anchors.front().in_x = last.in_x;
        subpath.anchors.front().in_y = last.in_y;
        subpath.anchors.pop_back();
      }
    }
    if (subpath.anchors.size() >= 2) {
      path.subpaths.push_back(std::move(subpath));
    }
    closed = true;
  }
  if (path.subpaths.empty()) {
    return std::nullopt;
  }
  return path;
}

// One decoded LDsc line descriptor. The LSty Data blob is a curve12_t (one
// f64 + four u8 flag bytes, per the afread headers); byte 2 of the four is
// the stroke panel's line-style control, pinned against ground truth July
// 2026: 0 = None (sprite wild file: black stroke defined but not drawn),
// 1 = Solid (the vec-* probe fixtures), 2 = Dashed (restaurant-menu-inside:
// Patn [4,2] at weight 8.33 renders 33/17 px dashes, so Patn values are
// stroke-width multiples exactly like Patchy's dash entries), 3 = textured
// brush (approximated as solid + notice). LDSa is the alignment control:
// 0 = Center (vec-*), 1 = Inside (restaurant-menu-inside), 2 = Outside (the
// remaining state of the three-way control). LDSc scales the width with the
// object transform.
struct AfLineStyle {
  double weight{0.0};
  std::uint8_t line_style{1};
  VectorStrokeAlignment alignment{VectorStrokeAlignment::Center};
  std::vector<double> dashes;      // stroke-width multiples
  double dash_phase{0.0};          // same units
  bool scale_with_object{false};
};

[[nodiscard]] std::optional<AfLineStyle> line_style_from_descriptor(
    const af::AfClass* line_descriptor) {
  if (line_descriptor == nullptr) {
    return std::nullopt;
  }
  const af::AfClass* style = line_descriptor->child_class(af::tag4("LDeL"));
  if (style == nullptr) {
    return std::nullopt;
  }
  AfLineStyle result;
  result.weight = style->double_field(af::tag4("Wght"), 0.0);
  if (const auto* data = style->field(af::tag4("Data"))) {
    if (const auto* blob = std::get_if<af::AfCurveArray>(&data->value);
        blob != nullptr && blob->bytes.size() >= 12) {
      result.line_style = blob->bytes[10];
    }
  }
  result.dashes = style->vec_field(af::tag4("Patn"));
  result.dash_phase = style->double_field(af::tag4("Phse"), 0.0);
  switch (line_descriptor->int_field(af::tag4("LDSa"), 0)) {
    case 1: result.alignment = VectorStrokeAlignment::Inside; break;
    case 2: result.alignment = VectorStrokeAlignment::Outside; break;
    default: result.alignment = VectorStrokeAlignment::Center; break;
  }
  result.scale_with_object = line_descriptor->bool_field(af::tag4("LDSc"), false);
  return result;
}

// A local-space vector path -> a real Patchy shape layer (the SVG import
// pattern: vector content + a baked raster; kLayerMetadataVectorShape +
// block-dirty so saves regenerate the PSD vector blocks). Shared by PCrv
// (hand-drawn curves) and ShpN (parametric shapes). Fill = BFFl, stroke paint
// = LIFl with width/style/alignment/dashes from LILn's line descriptor and
// the miter limit from the node's CnML; cap and join keep Patchy defaults
// (their wire enums are unmined - every observed file carries the identical
// default tuple). The node Xfrm maps local path coordinates into document
// space (full affine - vectors need no axis-aligned approximation). Returns
// nullopt when nothing is visible (placeholder path).
[[nodiscard]] std::optional<Layer> build_vector_layer_from_path(LayerBuildContext& ctx,
                                                                const af::AfClass& node,
                                                                const std::string& display,
                                                                VectorPath path) {
  double transform_scale = 1.0;
  if (const auto xfrm = node_xfrm(ctx, node); xfrm.size() == 6) {
    transform_vector_path(path, {xfrm[0], xfrm[3], xfrm[1], xfrm[4], xfrm[2], xfrm[5]});
    const double det = xfrm[0] * xfrm[4] - xfrm[1] * xfrm[3];
    if (std::isfinite(det) && std::abs(det) > 0.0) {
      transform_scale = std::sqrt(std::abs(det));
    }
  }
  VectorShapeContent content;
  content.path = std::move(path);
  float fill_alpha = 1.0F;
  bool brush_stroke_approximated = false;
  // The fill descriptor rides BFFl on current files; the oldest generation
  // (doc versions 3..9, the 2026-07-28 wild sweep) names the same field BFil
  // and stores the Fill class directly (no FDsc wrapper). Its stroke fields
  // are old-named too: the LDsc line descriptor rides a field tagged LSty
  // (same shape as LILn) and the stroke paint rides PFil (a direct Fill).
  const auto read_paint = [transform_scale, &brush_stroke_approximated](
                              const af::AfClass& source, VectorShapeContent& into,
                              float& alpha) -> bool {
    const af::AfClass* fill_descriptor = first_class_of(source, af::tag4("BFFl"));
    if (fill_descriptor == nullptr) {
      fill_descriptor = first_class_of(source, af::tag4("BFil"));
    }
    if (auto fill = vector_fill_from_descriptor(fill_descriptor)) {
      into.fill = std::move(fill->fill);
      alpha = fill->alpha;
    } else {
      into.fill.kind = VectorFillKind::None;
    }
    // LILn[0] (LDsc) -> LDeL: the field's value IS the LSty class.
    auto line = line_style_from_descriptor(first_class_of(source, af::tag4("LILn")));
    if (!line.has_value()) {
      line = line_style_from_descriptor(first_class_of(source, af::tag4("LSty")));
    }
    if (line.has_value() && line->weight > 0.0 && line->line_style != 0) {
      auto stroke_fill = vector_fill_from_descriptor(first_class_of(source, af::tag4("LIFl")));
      if (!stroke_fill.has_value() || stroke_fill->fill.kind == VectorFillKind::None) {
        stroke_fill = vector_fill_from_descriptor(first_class_of(source, af::tag4("PFil")));
      }
      if (stroke_fill.has_value() && stroke_fill->fill.kind != VectorFillKind::None) {
        into.stroke.enabled = true;
        into.stroke.width =
            line->scale_with_object ? line->weight * transform_scale : line->weight;
        into.stroke.alignment = line->alignment;
        into.stroke.content = std::move(stroke_fill->fill);
        into.stroke.opacity = std::clamp(static_cast<double>(stroke_fill->alpha), 0.0, 1.0);
        if (line->line_style == 2 && !line->dashes.empty()) {
          into.stroke.dashes = line->dashes;
          into.stroke.dash_offset = line->dash_phase;
        } else if (line->line_style == 3) {
          brush_stroke_approximated = true;
        }
        into.stroke.miter_limit =
            std::max(1.0, source.double_field(af::tag4("CnML"), into.stroke.miter_limit));
      }
    }
    return into.fill.kind != VectorFillKind::None || into.stroke.enabled;
  };
  bool painted = read_paint(node, content, fill_alpha);
  if (!painted) {
    // Symbol instances may store their paint on the defining sibling too
    // (beat-bot's linked raccoon members parse no local fill); chase the
    // instance-link rings the same way the geometry does.
    static constexpr const char* kRings[] = {"SLnk", "GLnk", "DLnk", "CLnk"};
    for (const char* ring : kRings) {
      const af::AfClass* link = node.child_class(tag_of(ring));
      const auto* members = link != nullptr ? class_list(*link, af::tag4("ILOb")) : nullptr;
      if (members == nullptr) {
        continue;
      }
      for (const auto& member : *members) {
        if (member != nullptr && member.get() != &node &&
            read_paint(*member, content, fill_alpha)) {
          painted = true;
          break;
        }
      }
      if (painted) {
        break;
      }
    }
  }
  if (!painted) {
    return std::nullopt;  // nothing visible; the honest placeholder says so
  }

  if (brush_stroke_approximated) {
    ctx.notices.push_back("Layer '" + display +
                          "': Affinity brush-textured stroke approximated as a solid stroke");
  }
  Layer layer(ctx.document.allocate_layer_id(), display, LayerKind::Pixel);
  layer.metadata()[kLayerMetadataVectorShape] = "1";
  mark_layer_vector_block_dirty(layer);  // no preserved PSD blocks: regenerate on save
  layer.set_vector_shape(std::move(content));
  apply_common(ctx, node, layer, display);
  if (fill_alpha < 1.0F) {
    layer.set_fill_opacity(layer.fill_opacity() * fill_alpha);
  }
  apply_mask_children(ctx, node, layer, display);
  update_vector_shape_raster(layer, Rect{0, 0, ctx.document.width(), ctx.document.height()},
                             nullptr);
  if (environment_variable_is_set("PATCHY_AF_TRACE")) {
    std::fprintf(stderr, "[af] vector '%s': baked %dx%d at %d,%d fill=%d stroke=%d\n",
                 display.c_str(), std::as_const(layer).pixels().width(),
                 std::as_const(layer).pixels().height(), layer.bounds().x, layer.bounds().y,
                 static_cast<int>(layer.vector_shape()->fill.kind),
                 layer.vector_shape()->stroke.enabled);
  }
  return layer;
}

// ---- Parametric shape nodes (ShpN) -> Patchy shape layers ----

// The cubic-bezier approximation constant for a quarter circle.
constexpr double kQuarterArcK = 0.5522847498307936;

// Corner-type enum on the wire (ShapeCornerType in the scripting API).
enum : int {
  kAfCornerRound = 0,
  kAfCornerStraight = 1,
  kAfCornerRoundInverse = 2,
  kAfCornerCutout = 3,
  kAfCornerNone = 4,
};

// Appends one rectangle corner's anchors to `subpath`, walking clockwise.
// `px`/`py` is the sharp corner point, `enter` the unit direction the path
// travels ALONG the incoming edge to reach it, `exit` the direction it leaves
// with, `radius` the corner size in local pixels (0 = sharp) and `type` the
// wire corner type. Geometry pinned by the shp-rect-* probe renders: Round is
// a circular quarter arc, Straight the chamfer between the two offset points,
// Cutout removes the radius-sized corner square, RoundInverse a concave
// quarter arc centered on the corner point itself.
void append_rect_corner(PathSubpath& subpath, double px, double py, double enter_x, double enter_y,
                        double exit_x, double exit_y, double radius, int type) {
  const auto add = [&](double x, double y, bool smooth) -> PathAnchor& {
    subpath.anchors.push_back(PathAnchor{x, y, x, y, x, y, smooth});
    return subpath.anchors.back();
  };
  if (radius <= 0.0 || type == kAfCornerNone) {
    add(px, py, false);
    return;
  }
  const double in_x = px - enter_x * radius;   // radius before the corner
  const double in_y = py - enter_y * radius;
  const double out_x = px + exit_x * radius;   // radius past the corner
  const double out_y = py + exit_y * radius;
  const double k = kQuarterArcK * radius;
  switch (type) {
    case kAfCornerRound: {
      PathAnchor& a = add(in_x, in_y, true);
      a.out_x = in_x + enter_x * k;
      a.out_y = in_y + enter_y * k;
      PathAnchor& b = add(out_x, out_y, true);
      b.in_x = out_x - exit_x * k;
      b.in_y = out_y - exit_y * k;
      break;
    }
    case kAfCornerRoundInverse: {
      PathAnchor& a = add(in_x, in_y, false);
      a.out_x = in_x + exit_x * k;
      a.out_y = in_y + exit_y * k;
      PathAnchor& b = add(out_x, out_y, false);
      b.in_x = out_x - enter_x * k;
      b.in_y = out_y - enter_y * k;
      break;
    }
    case kAfCornerCutout:
      add(in_x, in_y, false);
      add(in_x + out_x - px, in_y + out_y - py, false);
      add(out_x, out_y, false);
      break;
    case kAfCornerStraight:
    default:  // an unknown corner type degrades to its chamfer
      add(in_x, in_y, false);
      add(out_x, out_y, false);
      break;
  }
}

// Builds the local-space path for a parametric shape node's Shpe record, or
// nullopt for shape kinds Patchy does not model (the honest placeholder).
// Wire semantics pinned by the af-spike shp-* one-toggle probes (2026-07-28):
// the record's class tag is the shape kind; ShpB [x0,y0,x1,y1] is the local
// box the geometry fills; ShNR carries ShCR per-corner radii in
// TL/TR/BR/BL order (fraction of min(w,h), or pixels when AbSz), CTyp
// per-corner types, and Lock (absent/true = corner 0's radius AND type render
// on all four corners); ShpE is the inscribed ellipse; ShPy a regular Side-gon
// inscribed in the box ellipse, first vertex up.
//
// The 2026-07-29 long-tail kinds (diamond, trapezoid, pie, segment, crescent,
// heart, tear, arrow, double/square star, cog, cloud, curved-edge stars) are
// pinned by the af-spike shape-curves sweep: every kind was authored through
// the JS bridge at several parameter settings, Convert-to-Curves'd, and the
// exact baked beziers mined (corpus/shape-curves*.json); the constructions
// below reproduce those regions with Patchy's own arc segmentation. Radial
// kinds work in the box-inscribed ellipse's "unit space" (y down), where
// Affinity's own geometry is circles/arcs that the box then stretches.
[[nodiscard]] std::optional<VectorPath> parametric_shape_path(const af::AfClass& node,
                                                              std::string* why) {
  // A linked symbol instance keeps its geometry (Shpe record AND local box)
  // on the defining sibling; the instance's own transform still places it.
  const af::AfClass* geometry_node = &node;
  if (node.child_class(af::tag4("Shpe")) == nullptr) {
    if (const af::AfClass* sibling = symbol_sibling_with(node, af::tag4("Shpe"))) {
      geometry_node = sibling;
    }
  }
  const af::AfClass* shape = geometry_node->child_class(af::tag4("Shpe"));
  if (shape == nullptr) {
    *why = "has no shape record";
    return std::nullopt;
  }
  const auto box = geometry_node->vec_field(af::tag4("ShpB"));
  if (box.size() != 4 || !(box[2] > box[0]) || !(box[3] > box[1])) {
    *why = "has a degenerate shape box";
    return std::nullopt;
  }
  const double x0 = box[0];
  const double y0 = box[1];
  const double x1 = box[2];
  const double y1 = box[3];
  const double w = x1 - x0;
  const double h = y1 - y0;

  VectorPath path;
  PathSubpath subpath;
  subpath.closed = true;

  // Shared helpers for the long-tail kinds: unit space is the box-inscribed
  // ellipse's coordinate system (centre (0,0), radius 1, y down); mapping to
  // the document is an anisotropic scale, which keeps beziers beziers.
  constexpr double kPiSh = 3.14159265358979323846;
  constexpr double kCircleK = 0.5522847498307934;
  const double ucx = (x0 + x1) / 2.0;
  const double ucy = (y0 + y1) / 2.0;
  const double hw = w / 2.0;
  const double hh = h / 2.0;
  const auto unit_x = [&](double ux) { return ucx + hw * ux; };
  const auto unit_y = [&](double uy) { return ucy + hh * uy; };
  const auto push_corner = [](PathSubpath& sp, double px, double py) {
    sp.anchors.push_back(PathAnchor{px, py, px, py, px, py, false});
  };
  const auto push_corner_unit = [&](PathSubpath& sp, double ux, double uy) {
    push_corner(sp, unit_x(ux), unit_y(uy));
  };
  // Append a circular arc (unit space: centre (ax,ay), radius r) from angle
  // a0 to a1, either direction, split into <=90-degree bezier segments. When
  // the subpath already ends at the arc's start point only its out handle is
  // set (so lines and arcs chain); interior anchors are smooth, the arc's
  // endpoints stay corners for the caller to join.
  const auto append_unit_arc = [&](PathSubpath& sp, double ax, double ay, double r, double a0,
                                   double a1) {
    const int segments =
        std::max(1, static_cast<int>(std::ceil(std::abs(a1 - a0) / (kPiSh / 3.0) - 1e-9)));
    const double step = (a1 - a0) / segments;
    const double k = 4.0 / 3.0 * std::tan(step / 4.0) * r;
    for (int i = 0; i <= segments; ++i) {
      const double ang = a0 + step * static_cast<double>(i);
      const double px = unit_x(ax + r * std::cos(ang));
      const double py = unit_y(ay + r * std::sin(ang));
      const double tx = -std::sin(ang) * k * hw;
      const double ty = std::cos(ang) * k * hh;
      if (i == 0) {
        if (!sp.anchors.empty() && std::abs(sp.anchors.back().anchor_x - px) < 1e-6 &&
            std::abs(sp.anchors.back().anchor_y - py) < 1e-6) {
          sp.anchors.back().out_x = px + tx;
          sp.anchors.back().out_y = py + ty;
        } else {
          sp.anchors.push_back(PathAnchor{px, py, px, py, px + tx, py + ty, false});
        }
        continue;
      }
      PathAnchor anchor{px, py, px - tx, py - ty, px, py, i < segments};
      if (i < segments) {
        anchor.out_x = px + tx;
        anchor.out_y = py + ty;
      }
      sp.anchors.push_back(anchor);
    }
  };
  // A closed subpath built from chained arcs ends where it began; fold the
  // duplicate final anchor's incoming handle onto the first anchor.
  const auto close_fold = [](PathSubpath& sp) {
    if (sp.anchors.size() >= 2) {
      const auto& first = sp.anchors.front();
      const auto& last = sp.anchors.back();
      if (std::abs(first.anchor_x - last.anchor_x) < 1e-6 &&
          std::abs(first.anchor_y - last.anchor_y) < 1e-6) {
        sp.anchors.front().in_x = last.in_x;
        sp.anchors.front().in_y = last.in_y;
        sp.anchors.pop_back();
      }
    }
  };
  subpath.op = PathCombineOp::Add;
  subpath.shape_group = 0;

  if (shape->type_tag == af::tag4("ShNR")) {
    auto radii = shape->vec_field(af::tag4("ShCR"));
    radii.resize(4, 0.0);
    auto types_raw = shape->vec_field(af::tag4("CTyp"));
    types_raw.resize(4, static_cast<double>(kAfCornerNone));
    std::array<int, 4> types{};
    for (std::size_t i = 0; i < 4; ++i) {
      types[i] = static_cast<int>(types_raw[i]);
    }
    if (shape->bool_field(af::tag4("Lock"), true)) {
      // Single-radius mode: corner 0 defines every corner.
      radii[1] = radii[2] = radii[3] = radii[0];
      types[1] = types[2] = types[3] = types[0];
    }
    const bool absolute = shape->bool_field(af::tag4("AbSz"), false);
    const double reference = std::min(w, h);
    std::array<double, 4> r{};  // TL, TR, BR, BL in local pixels
    for (std::size_t i = 0; i < 4; ++i) {
      const double value = absolute ? radii[i] : radii[i] * reference;
      r[i] = (types[i] == kAfCornerNone) ? 0.0 : std::max(0.0, value);
    }
    // Adjacent radii cannot exceed their shared edge (the CSS overlap rule;
    // Affinity's exact overflow behavior is unprobed).
    double scale = 1.0;
    const auto limit = [&](double a, double b, double edge) {
      if (a + b > edge && a + b > 0.0) {
        scale = std::min(scale, edge / (a + b));
      }
    };
    limit(r[0], r[1], w);
    limit(r[1], r[2], h);
    limit(r[2], r[3], w);
    limit(r[3], r[0], h);
    for (double& value : r) {
      value *= scale;
    }
    append_rect_corner(subpath, x0, y0, 0.0, -1.0, 1.0, 0.0, r[0], types[0]);
    append_rect_corner(subpath, x1, y0, 1.0, 0.0, 0.0, 1.0, r[1], types[1]);
    append_rect_corner(subpath, x1, y1, 0.0, 1.0, -1.0, 0.0, r[2], types[2]);
    append_rect_corner(subpath, x0, y1, -1.0, 0.0, 0.0, -1.0, r[3], types[3]);
  } else if (shape->type_tag == af::tag4("ShpE")) {
    const double cx = (x0 + x1) / 2.0;
    const double cy = (y0 + y1) / 2.0;
    const double kx = kQuarterArcK * (w / 2.0);
    const double ky = kQuarterArcK * (h / 2.0);
    const auto add = [&](double ax, double ay, double ix, double iy, double ox, double oy) {
      subpath.anchors.push_back(PathAnchor{ax, ay, ix, iy, ox, oy, true});
    };
    add(cx, y0, cx - kx, y0, cx + kx, y0);
    add(x1, cy, x1, cy - ky, x1, cy + ky);
    add(cx, y1, cx + kx, y1, cx - kx, y1);
    add(x0, cy, x0, cy + ky, x0, cy - ky);
  } else if (shape->type_tag == af::tag4("ShPy")) {
    for (const auto& field : shape->fields) {
      if (field.tag != af::tag4("Side") && field.tag != af::tag4("Smth") &&
          field.tag != af::tag4("Curv")) {
        *why = "is a non-regular Affinity polygon";
        return std::nullopt;
      }
    }
    const auto sides = std::clamp<std::int64_t>(shape->int_field(af::tag4("Side"), 5), 3, 256);
    const bool smooth = shape->bool_field(af::tag4("Smth"), false);
    const double cx = (x0 + x1) / 2.0;
    const double cy = (y0 + y1) / 2.0;
    constexpr double kPi = 3.14159265358979323846;
    // Smoothed polygons replace every corner with a symmetric smooth anchor
    // whose tangent length scales with Curv: the shp-polygon6-smooth probe
    // pins Smth=true/Curv=0 as rendering EXACTLY like the plain polygon, so
    // Curv is the rounding amount. Curv=1 is mapped to the circle through
    // the vertices ((4/3)tan(pi/2n) in unit-circle space) - plausible but
    // unpinned, no Curv>0 ground truth exists yet.
    const double curv = std::clamp(shape->double_field(af::tag4("Curv"), 0.0), 0.0, 1.0);
    const double tangent =
        smooth ? curv * (4.0 / 3.0) * std::tan(kPi / (2.0 * static_cast<double>(sides))) : 0.0;
    for (std::int64_t i = 0; i < sides; ++i) {
      const double angle = -kPi / 2.0 + 2.0 * kPi * static_cast<double>(i) /
                                            static_cast<double>(sides);
      const double ux = std::cos(angle);
      const double uy = std::sin(angle);
      const double x = cx + (w / 2.0) * ux;
      const double y = cy + (h / 2.0) * uy;
      if (!smooth) {
        subpath.anchors.push_back(PathAnchor{x, y, x, y, x, y, false});
        continue;
      }
      // The tangent runs perpendicular to the radius (unit space), scaled per
      // axis by the box's half sizes.
      const double tx = -uy * tangent * (w / 2.0);
      const double ty = ux * tangent * (h / 2.0);
      subpath.anchors.push_back(PathAnchor{x, y, x - tx, y - ty, x + tx, y + ty, true});
    }
  } else if (shape->type_tag == af::tag4("ShSt")) {
    // Star: Pnts outer vertices on the box-inscribed ellipse (first vertex
    // up, like ShPy), alternating with inner vertices at the IRad fraction,
    // offset by half a step. CrvL/CrvR only take effect when Lgcy
    // (curvedEdges) is true - the shape-curves sweep pins both a plain-star
    // conversion for Lgcy alone or CrvL/CrvR alone, and tangential tip
    // handles for the combination (star-lgcy-crv, CrvL=CrvR=0.4 -> unit
    // handle 0.18665). Circle-rounded stars (CrcI/CrcO) keep the
    // placeholder until their tangent construction is pinned.
    if (shape->double_field(af::tag4("CrcI"), 0.0) > 1e-6 ||
        shape->double_field(af::tag4("CrcO"), 0.0) > 1e-6) {
      *why = "is a circle-rounded Affinity star";
      return std::nullopt;
    }
    const auto points = std::clamp<std::int64_t>(shape->int_field(af::tag4("Pnts"), 5), 3, 256);
    const double inner = std::clamp(shape->double_field(af::tag4("IRad"), 0.5), 0.0, 1.0);
    const bool curved = shape->bool_field(af::tag4("Lgcy"), false);
    const double curve_left =
        curved ? std::clamp(shape->double_field(af::tag4("CrvL"), 0.0), 0.0, 1.0) : 0.0;
    const double curve_right =
        curved ? std::clamp(shape->double_field(af::tag4("CrvR"), 0.0), 0.0, 1.0) : 0.0;
    // The pinned 0.4 -> 0.18665 sample gives the per-unit-curve handle scale
    // at five points; scale other point counts by their half-step tangent.
    const double handle_scale = 0.46662 * std::tan(kPiSh / static_cast<double>(points)) /
                                std::tan(kPiSh / 5.0);
    const double cx = (x0 + x1) / 2.0;
    const double cy = (y0 + y1) / 2.0;
    constexpr double kPi = 3.14159265358979323846;
    for (std::int64_t i = 0; i < points; ++i) {
      const double outer_angle = -kPi / 2.0 + 2.0 * kPi * static_cast<double>(i) /
                                                  static_cast<double>(points);
      const double inner_angle = outer_angle + kPi / static_cast<double>(points);
      const double ox = cx + (w / 2.0) * std::cos(outer_angle);
      const double oy = cy + (h / 2.0) * std::sin(outer_angle);
      if (curve_left > 0.0 || curve_right > 0.0) {
        // Tangential handles round the tip; CrvL feeds the incoming side.
        const double tx = -std::sin(outer_angle) * hw;
        const double ty = std::cos(outer_angle) * hh;
        subpath.anchors.push_back(PathAnchor{
            ox, oy, ox - tx * curve_left * handle_scale, oy - ty * curve_left * handle_scale,
            ox + tx * curve_right * handle_scale, oy + ty * curve_right * handle_scale, false});
      } else {
        subpath.anchors.push_back(PathAnchor{ox, oy, ox, oy, ox, oy, false});
      }
      const double ix = cx + inner * (w / 2.0) * std::cos(inner_angle);
      const double iy = cy + inner * (h / 2.0) * std::sin(inner_angle);
      subpath.anchors.push_back(PathAnchor{ix, iy, ix, iy, ix, iy, false});
    }
  } else if (shape->type_tag == af::tag4("ShpD")) {
    // Diamond: side vertices sit Pos of the way up the box (default 0.5).
    const double pos = std::clamp(shape->double_field(af::tag4("Pos "), 0.5), 0.0, 1.0);
    const double side_y = y1 - pos * h;
    push_corner(subpath, ucx, y0);
    push_corner(subpath, x1, side_y);
    push_corner(subpath, ucx, y1);
    push_corner(subpath, x0, side_y);
  } else if (shape->type_tag == af::tag4("ShTz")) {
    // Trapezoid: the top edge runs from PosL to PosR across the box.
    const double pos_l = std::clamp(shape->double_field(af::tag4("PosL"), 0.25), 0.0, 1.0);
    const double pos_r = std::clamp(shape->double_field(af::tag4("PosR"), 0.75), 0.0, 1.0);
    push_corner(subpath, x1, y1);
    push_corner(subpath, x0, y1);
    push_corner(subpath, x0 + pos_l * w, y0);
    push_corner(subpath, x0 + pos_r * w, y0);
  } else if (shape->type_tag == af::tag4("ShHt")) {
    // Heart: a fixed six-anchor template in box fractions; only the top
    // cleft anchor moves - its y is Sprd of the way down the box.
    const double spread = std::clamp(shape->double_field(af::tag4("Sprd"), 0.2), 0.0, 1.0);
    struct HeartAnchor {
      double ax, ay, inx, iny, outx, outy;
      bool smooth;
    };
    const HeartAnchor kHeart[6] = {
        {0.5, 0.0, 0.3947370, 0.0, 0.6052630, 0.0, false},
        {0.9210530, 0.1, 0.8157890, 0.0, 1.0263160, 0.2, true},
        {0.9210530, 0.6, 1.0263160, 0.4, 0.8473680, 0.75, true},
        {0.5, 1.0, 0.6578950, 0.9, 0.3421050, 0.9, false},
        {0.0789474, 0.6, 0.1526320, 0.75, -0.0263158, 0.4, true},
        {0.0789474, 0.1, -0.0263158, 0.2, 0.1842110, 0.0, true},
    };
    for (const auto& a : kHeart) {
      const double ay = a.ax == 0.5 && a.ay == 0.0 ? spread : a.ay;
      subpath.anchors.push_back(PathAnchor{x0 + a.ax * w, y0 + ay * h, x0 + a.inx * w,
                                           y0 + a.iny * h, x0 + a.outx * w, y0 + a.outy * h,
                                           a.smooth});
    }
  } else if (shape->type_tag == af::tag4("ShTr")) {
    // Tear: an ellipse whose top anchor collapses into a corner at Tail
    // across the top edge; Curv scales the upper side handles (default 0.3),
    // Bend curls the tail tip (handle template pinned at Tail 0.5). The Fixd
    // and Ball fields do not alter 3.x geometry (sweep-pinned) and are
    // ignored.
    const double tail = std::clamp(shape->double_field(af::tag4("Tail"), 0.5), 0.0, 1.0);
    const double curve = std::clamp(shape->double_field(af::tag4("Curv"), 0.3), 0.0, 1.0);
    const double bend = std::clamp(shape->double_field(af::tag4("Bend"), 0.0), -1.0, 1.0);
    const double tip_x = x0 + tail * w;
    // The bottom bulb is a half-ellipse whose vertical radius caps at half
    // the WIDTH (the tall-box probe pins side anchors at y1 - w/2 there);
    // the side anchors' upper handles are Curv of the way back up to the top.
    const double bulb = std::min(hw, hh);
    const double side_y = y1 - bulb;
    subpath.anchors.push_back(PathAnchor{tip_x, y0, tip_x + bend * w * 0.1,
                                         y0 + bend * h * 0.25, tip_x + bend * w * 0.25, y0,
                                         false});
    subpath.anchors.push_back(PathAnchor{x1, side_y, x1, side_y - curve * (side_y - y0), x1,
                                         side_y + kCircleK * bulb, true});
    subpath.anchors.push_back(PathAnchor{ucx, y1, ucx + kCircleK * hw, y1, ucx - kCircleK * hw,
                                         y1, true});
    subpath.anchors.push_back(PathAnchor{x0, side_y, x0, side_y + kCircleK * bulb, x0,
                                         side_y - curve * (side_y - y0), true});
  } else if (shape->type_tag == af::tag4("ShDA")) {
    // Arrow: a straight-line polygon. Head length is LPr1/RPr1 of the BOX
    // HEIGHT (wide-box pinned), the shaft is Thck of the height, and
    // LPr2/RPr2 offset the shaft junction from the barb base. Only end
    // styles 0 (flat) and 1 (the plain arrowhead) are modeled.
    const auto left_style = enum_field_id(*shape, af::tag4("LSty"), 1);
    const auto right_style = enum_field_id(*shape, af::tag4("RSty"), 1);
    if (left_style > 1 || right_style > 1) {
      *why = "uses an Affinity arrow end style Patchy does not model";
      return std::nullopt;
    }
    const double thick = std::clamp(shape->double_field(af::tag4("Thck"), 0.35), 0.0, 1.0);
    const double shaft_top = ucy - thick * hh;
    const double shaft_bottom = ucy + thick * hh;
    double left_len = std::max(0.0, shape->double_field(af::tag4("LPr1"), 0.5)) * h;
    const double left_inner = shape->double_field(af::tag4("LPr2"), 0.0) * h;
    double right_len = std::max(0.0, shape->double_field(af::tag4("RPr1"), 0.5)) * h;
    const double right_inner = shape->double_field(af::tag4("RPr2"), 0.0) * h;
    // Heads that would overlap scale down proportionally to meet (the
    // tall-box probe: two 30px heads in a 44px box become 22px each).
    const double head_total = (left_style == 1 ? left_len : 0.0) +
                              (right_style == 1 ? right_len : 0.0);
    if (head_total > w && head_total > 0.0) {
      left_len *= w / head_total;
      right_len *= w / head_total;
    }
    const double barb_l = x0 + left_len;
    const double shaft_l = left_style == 1 ? barb_l + left_inner : x0;
    const double barb_r = x1 - right_len;
    const double shaft_r = right_style == 1 ? barb_r - right_inner : x1;
    if (left_style == 1) {
      push_corner(subpath, shaft_l, shaft_bottom);
      push_corner(subpath, barb_l, y1);
      push_corner(subpath, x0, ucy);
      push_corner(subpath, barb_l, y0);
      push_corner(subpath, shaft_l, shaft_top);
    } else {
      push_corner(subpath, x0, shaft_bottom);
      push_corner(subpath, x0, shaft_top);
    }
    if (right_style == 1) {
      push_corner(subpath, shaft_r, shaft_top);
      push_corner(subpath, barb_r, y0);
      push_corner(subpath, x1, ucy);
      push_corner(subpath, barb_r, y1);
      push_corner(subpath, shaft_r, shaft_bottom);
    } else {
      push_corner(subpath, x1, shaft_top);
      push_corner(subpath, x1, shaft_bottom);
    }
  } else if (shape->type_tag == af::tag4("ShPi")) {
    // Pie: the wire angles are radians in y-UP math orientation (AngS
    // default pi/2 = top, AngE default 0 = right); the filled sector runs
    // from AngE to AngS clockwise on screen. IRad > 0 cuts an annulus.
    const double phi_s = -shape->double_field(af::tag4("AngS"), kPiSh / 2.0);
    double phi_e = -shape->double_field(af::tag4("AngE"), 0.0);
    const double inner = std::clamp(shape->double_field(af::tag4("IRad"), 0.0), 0.0, 1.0);
    double sweep = phi_s - phi_e;
    sweep -= std::floor(sweep / (2.0 * kPiSh)) * 2.0 * kPiSh;  // wrap into (0, 2*pi]
    if (sweep < 1e-9) {
      sweep = 2.0 * kPiSh;
    }
    const double phi_end = phi_e + sweep;  // == phi_s modulo 2*pi
    if (inner <= 1e-6) {
      push_corner_unit(subpath, std::cos(phi_end), std::sin(phi_end));
      push_corner_unit(subpath, 0.0, 0.0);
      append_unit_arc(subpath, 0.0, 0.0, 1.0, phi_e, phi_end);
    } else {
      push_corner_unit(subpath, std::cos(phi_end), std::sin(phi_end));
      append_unit_arc(subpath, 0.0, 0.0, inner, phi_end, phi_e);
      append_unit_arc(subpath, 0.0, 0.0, 1.0, phi_e, phi_end);
    }
    close_fold(subpath);
  } else if (shape->type_tag == af::tag4("ShSg")) {
    // Segment: the box ellipse clipped to the band between two chords
    // perpendicular to the Angl direction (unit space): Pos0/Pos1 map to
    // signed offsets 2*Pos-1 along the direction.
    const double angle = shape->double_field(af::tag4("Angl"), kPiSh / 2.0);
    double c0 = std::clamp(2.0 * shape->double_field(af::tag4("Pos0"), 0.25) - 1.0, -1.0, 1.0);
    double c1 = std::clamp(2.0 * shape->double_field(af::tag4("Pos1"), 1.0) - 1.0, -1.0, 1.0);
    if (c0 > c1) {
      std::swap(c0, c1);
    }
    const double alpha0 = std::acos(std::clamp(c0, -1.0, 1.0));
    const double alpha1 = std::acos(std::clamp(c1, -1.0, 1.0));
    if (alpha0 - alpha1 < 1e-6) {
      *why = "has a degenerate shape outline";
      return std::nullopt;
    }
    const double delta = -angle;  // screen angle of the cut direction
    push_corner_unit(subpath, std::cos(delta + alpha0), std::sin(delta + alpha0));
    push_corner_unit(subpath, std::cos(delta - alpha0), std::sin(delta - alpha0));
    if (alpha1 > 1e-6) {
      append_unit_arc(subpath, 0.0, 0.0, 1.0, delta - alpha0, delta - alpha1);
      push_corner_unit(subpath, std::cos(delta + alpha1), std::sin(delta + alpha1));
      append_unit_arc(subpath, 0.0, 0.0, 1.0, delta + alpha1, delta + alpha0);
    } else {
      append_unit_arc(subpath, 0.0, 0.0, 1.0, delta - alpha0, delta + alpha0);
    }
    close_fold(subpath);
  } else if (shape->type_tag == af::tag4("ShCr")) {
    // Crescent: the region between two boundary curves (unit space), each
    // from (0,-1) through (Arc, 0) to (0,1) - ArcL/ArcR are the signed bulge
    // fractions (the JS setters negate leftArc on the wire; the wire values
    // place both boundaries directly). Affinity's boundary is two cubics
    // whose handles the shape-curves sweep pins exactly across four bulges:
    // the endpoint handle is (K*m, (1-|m|)/3) and the mid handle is vertical
    // with length K*|m| + (1-|m|)/3 - the ellipse half at |m|=1, the
    // straight line at m=0, blended linearly between.
    const double arc_l = std::clamp(shape->double_field(af::tag4("ArcL"), -1.0), -1.0, 1.0);
    const double arc_r = std::clamp(shape->double_field(af::tag4("ArcR"), -0.3), -1.0, 1.0);
    if (std::abs(arc_r - arc_l) < 1e-6) {
      *why = "has a degenerate shape outline";
      return std::nullopt;
    }
    // Emits the boundary for bulge m from (0,-1) to (0,1) when down is true,
    // or the reverse; the subpath's last anchor is the start point.
    const auto boundary = [&](double m, bool down) {
      const double s = down ? 1.0 : -1.0;
      if (std::abs(m) < 1e-6) {
        push_corner_unit(subpath, 0.0, s);
        return;
      }
      const double end_off = (1.0 - std::abs(m)) / 3.0;
      const double mid_len = kCircleK * std::abs(m) + end_off;
      auto& start = subpath.anchors.back();
      start.out_x = unit_x(kCircleK * m);
      start.out_y = unit_y(-s + s * end_off);
      subpath.anchors.push_back(PathAnchor{unit_x(m), unit_y(0.0), unit_x(m),
                                           unit_y(-s * mid_len), unit_x(m), unit_y(s * mid_len),
                                           true});
      subpath.anchors.push_back(PathAnchor{unit_x(0.0), unit_y(s), unit_x(kCircleK * m),
                                           unit_y(s - s * end_off), unit_x(0.0), unit_y(s),
                                           false});
    };
    push_corner_unit(subpath, 0.0, -1.0);
    boundary(arc_r, true);
    boundary(arc_l, false);
    close_fold(subpath);
  } else if (shape->type_tag == af::tag4("ShDS")) {
    // Double star: 4*Pnts straight-line vertices at uniform angle steps from
    // the top, radii cycling [1, IRad, PRad, IRad].
    const auto points = std::clamp<std::int64_t>(shape->int_field(af::tag4("Pnts"), 5), 2, 128);
    const double inner = std::clamp(shape->double_field(af::tag4("IRad"), 0.525731), 0.0, 1.0);
    const double point_r = std::clamp(shape->double_field(af::tag4("PRad"), 0.809017), 0.0, 1.0);
    const double radii[4] = {1.0, inner, point_r, inner};
    const std::int64_t count = points * 4;
    for (std::int64_t i = 0; i < count; ++i) {
      const double ang = -kPiSh / 2.0 +
                         2.0 * kPiSh * static_cast<double>(i) / static_cast<double>(count);
      const double r = radii[i % 4];
      push_corner_unit(subpath, r * std::cos(ang), r * std::sin(ang));
    }
  } else if (shape->type_tag == af::tag4("ShSS")) {
    // Square star: Side arms whose tips are flattened; per arm the two outer
    // corners sit at (cos h, +/- (1-COut)*sin h) in tip-local unit space
    // (h = pi/Side), with the inner corner between arms at radius 1-COut.
    const auto sides = std::clamp<std::int64_t>(shape->int_field(af::tag4("Side"), 5), 3, 128);
    const double cutout = std::clamp(shape->double_field(af::tag4("COut"), 0.5), 0.0, 1.0);
    const double inner_r = 1.0 - cutout;
    const double half = kPiSh / static_cast<double>(sides);
    for (std::int64_t k = 0; k < sides; ++k) {
      const double tip = -kPiSh / 2.0 + half +
                         2.0 * kPiSh * static_cast<double>(k) / static_cast<double>(sides);
      const double ct = std::cos(tip);
      const double st = std::sin(tip);
      const double px = std::cos(half);
      const double py = inner_r * std::sin(half);
      push_corner_unit(subpath, ct * px - st * -py, st * px + ct * -py);
      push_corner_unit(subpath, ct * px - st * py, st * px + ct * py);
      push_corner_unit(subpath, inner_r * std::cos(tip + half), inner_r * std::sin(tip + half));
    }
  } else if (shape->type_tag == af::tag4("ShCg")) {
    // Cog: alternating tooth-top arcs (radius 1, width TtSz of the tooth
    // period) and root arcs (radius IRad, width NtSz), joined by straight
    // flanks; Curv bows the flanks (handles at 2/3*Curv along the chord,
    // approximate); Hole cuts a centre ellipse.
    const auto teeth = std::clamp<std::int64_t>(shape->int_field(af::tag4("Teth"), 12), 3, 256);
    const double root_r = std::clamp(shape->double_field(af::tag4("IRad"), 0.85), 0.0, 1.0);
    const double hole = std::clamp(shape->double_field(af::tag4("Hole"), 0.2), 0.0, 1.0);
    const double tooth_size = std::clamp(shape->double_field(af::tag4("TtSz"), 0.37), 0.0, 1.0);
    const double notch_size = std::clamp(shape->double_field(af::tag4("NtSz"), 0.42), 0.0, 1.0);
    const double curve = std::clamp(shape->double_field(af::tag4("Curv"), 0.0), 0.0, 1.0);
    const double period = 2.0 * kPiSh / static_cast<double>(teeth);
    const double half_top = tooth_size * period / 2.0;
    const double half_notch = notch_size * period / 2.0;
    for (std::int64_t k = 0; k < teeth; ++k) {
      const double centre = -kPiSh / 2.0 + period * static_cast<double>(k);
      append_unit_arc(subpath, 0.0, 0.0, 1.0, centre - half_top, centre + half_top);
      append_unit_arc(subpath, 0.0, 0.0, root_r, centre + period / 2.0 - half_notch,
                      centre + period / 2.0 + half_notch);
    }
    close_fold(subpath);
    if (curve > 0.0) {
      // Bow every straight flank: a flank runs between an arc-end anchor
      // (degenerate out handle) and the next arc-start anchor.
      const std::size_t n = subpath.anchors.size();
      for (std::size_t i = 0; i < n; ++i) {
        auto& a = subpath.anchors[i];
        auto& b = subpath.anchors[(i + 1) % n];
        const bool a_flat = a.out_x == a.anchor_x && a.out_y == a.anchor_y;
        const bool b_flat = b.in_x == b.anchor_x && b.in_y == b.anchor_y;
        if (a_flat && b_flat) {
          const double dx = (b.anchor_x - a.anchor_x) * (2.0 / 3.0) * curve;
          const double dy = (b.anchor_y - a.anchor_y) * (2.0 / 3.0) * curve;
          a.out_x = a.anchor_x + dx;
          a.out_y = a.anchor_y + dy;
          b.in_x = b.anchor_x - dx;
          b.in_y = b.anchor_y - dy;
        }
      }
    }
    if (subpath.anchors.size() < 2) {
      *why = "has a degenerate shape outline";
      return std::nullopt;
    }
    path.subpaths.push_back(std::move(subpath));
    if (hole > 1e-6) {
      PathSubpath hole_path;
      hole_path.closed = true;
      hole_path.op = PathCombineOp::Add;
      hole_path.shape_group = 0;
      append_unit_arc(hole_path, 0.0, 0.0, hole, -kPiSh / 2.0, 3.0 * kPiSh / 2.0);
      close_fold(hole_path);
      path.subpaths.push_back(std::move(hole_path));
    }
    return path;
  } else if (shape->type_tag == af::tag4("ShCl")) {
    // Cloud: Bubl bubbles; notches at radius IRad between bumps at radius 1.
    // Each bubble half is a quarter-ellipse in the bump-rotated frame with
    // radial semi-axis 1 - IRad*cos(P/2) and tangential semi-axis
    // IRad*sin(P/2) (wide- and default-probe pinned).
    const auto bubbles = std::clamp<std::int64_t>(shape->int_field(af::tag4("Bubl"), 12), 3, 256);
    const double inner = std::clamp(shape->double_field(af::tag4("IRad"), 0.8164966), 0.01, 1.0);
    const double period = 2.0 * kPiSh / static_cast<double>(bubbles);
    const double radial = std::max(0.0, 1.0 - inner * std::cos(period / 2.0));
    const double tangential = inner * std::sin(period / 2.0);
    for (std::int64_t k = 0; k < bubbles; ++k) {
      const double bump = -kPiSh / 2.0 + period * static_cast<double>(k);
      const double dirx = std::cos(bump);
      const double diry = std::sin(bump);
      const double tanx = -std::sin(bump);
      const double tany = std::cos(bump);
      const double n1x = inner * std::cos(bump - period / 2.0);
      const double n1y = inner * std::sin(bump - period / 2.0);
      const double n2x = inner * std::cos(bump + period / 2.0);
      const double n2y = inner * std::sin(bump + period / 2.0);
      const auto push_or_patch = [&](double ux, double uy, double outx, double outy) {
        const double px = unit_x(ux);
        const double py = unit_y(uy);
        if (!subpath.anchors.empty() && std::abs(subpath.anchors.back().anchor_x - px) < 1e-6 &&
            std::abs(subpath.anchors.back().anchor_y - py) < 1e-6) {
          subpath.anchors.back().out_x = unit_x(outx);
          subpath.anchors.back().out_y = unit_y(outy);
        } else {
          subpath.anchors.push_back(PathAnchor{px, py, px, py, unit_x(outx), unit_y(outy),
                                               false});
        }
      };
      push_or_patch(n1x, n1y, n1x + kCircleK * radial * dirx, n1y + kCircleK * radial * diry);
      subpath.anchors.push_back(PathAnchor{
          unit_x(dirx), unit_y(diry), unit_x(dirx - kCircleK * tangential * tanx),
          unit_y(diry - kCircleK * tangential * tany),
          unit_x(dirx + kCircleK * tangential * tanx),
          unit_y(diry + kCircleK * tangential * tany), true});
      subpath.anchors.push_back(PathAnchor{unit_x(n2x), unit_y(n2y),
                                           unit_x(n2x + kCircleK * radial * dirx),
                                           unit_y(n2y + kCircleK * radial * diry), unit_x(n2x),
                                           unit_y(n2y), false});
    }
    close_fold(subpath);
  } else if (shape->type_tag == af::tag4("ShpT")) {
    // Triangle: apex at the "Pos " fraction across the top edge (default
    // centered), base along the bottom of the box.
    for (const auto& field : shape->fields) {
      if (field.tag != af::tag4("Pos ")) {
        *why = "is an Affinity triangle variant Patchy does not model";
        return std::nullopt;
      }
    }
    const double pos = std::clamp(shape->double_field(af::tag4("Pos "), 0.5), 0.0, 1.0);
    const double ax = x0 + pos * w;
    subpath.anchors.push_back(PathAnchor{ax, y0, ax, y0, ax, y0, false});
    subpath.anchors.push_back(PathAnchor{x1, y1, x1, y1, x1, y1, false});
    subpath.anchors.push_back(PathAnchor{x0, y1, x0, y1, x0, y1, false});
  } else {
    *why = "is an Affinity shape kind Patchy does not model";
    return std::nullopt;
  }

  if (subpath.anchors.size() < 2) {
    *why = "has a degenerate shape outline";
    return std::nullopt;
  }
  path.subpaths.push_back(std::move(subpath));
  return path;
}

}  // namespace
// PCrv -> shape layer via its stored poly-curve.
[[nodiscard]] std::optional<Layer> build_vector_layer(LayerBuildContext& ctx, const af::AfClass& node,
                                                      const std::string& display) {
  auto path = vector_path_from_node(node);
  if (environment_variable_is_set("PATCHY_AF_TRACE")) {
    std::fprintf(stderr, "[af] vector '%s': path=%d subpaths=%zu crvs=%d data=%d\n",
                 display.c_str(), path.has_value(),
                 path.has_value() ? path->subpaths.size() : 0U,
                 node.child_class(af::tag4("Crvs")) != nullptr,
                 node.child_class(af::tag4("Crvs")) != nullptr &&
                     node.child_class(af::tag4("Crvs"))->child_class(af::tag4("Data")) != nullptr);
  }
  if (!path.has_value()) {
    return std::nullopt;
  }
  return build_vector_layer_from_path(ctx, node, display, std::move(*path));
}
// Collect the geometry of a vector-mask adjunct subtree (declared above
// apply_mask_children): a PCrv/ShpN/Comp node contributes its path, a Grup
// recurses its children; every contributor's LOCAL path maps through its own
// Xfrm composed onto the running affine, and each contributor takes its own
// shape_group with PathCombineOp::Add - the union-of-even-odd-shapes rule the
// SVG importer's clipPath handling established. Hidden contributors are
// skipped. `appended`/`unsupported` let the caller distinguish "not vector
// geometry at all" (stay silent) from a partly-decodable mask (drop whole,
// with a notice - a partial mask would clip wrongly).
void append_vector_mask_paths(const af::AfClass& node, const std::array<double, 6>& base,
                              int depth, VectorPath& into, std::int32_t& group,
                              int& appended, int& unsupported) {
  if (depth > 16) {
    ++unsupported;
    return;
  }
  if (!node.bool_field(af::tag4("Visi"), true)) {
    return;
  }
  auto composed = base;
  if (const auto xfrm = node.vec_field(af::tag4("Xfrm")); xfrm.size() == 6) {
    std::array<double, 6> own{};
    std::copy(xfrm.begin(), xfrm.end(), own.begin());
    composed = compose_transforms(base, own);
  }
  auto path = vector_path_from_node(node);
  if (!path.has_value() && node.type_tag == af::tag4("ShpN")) {
    std::string why;
    path = parametric_shape_path(node, &why);
  }
  if (path.has_value()) {
    transform_vector_path(
        *path, {composed[0], composed[3], composed[1], composed[4], composed[2], composed[5]});
    for (auto& subpath : path->subpaths) {
      subpath.shape_group = group;
      subpath.op = PathCombineOp::Add;
      into.subpaths.push_back(std::move(subpath));
    }
    ++group;
    ++appended;
    return;
  }
  if (const auto* kids = class_list(node, af::tag4("Chld")); kids != nullptr && !kids->empty()) {
    for (const auto& child : *kids) {
      if (child != nullptr) {
        append_vector_mask_paths(*child, composed, depth + 1, into, group, appended, unsupported);
      }
    }
    return;
  }
  ++unsupported;
}
// ShpN -> shape layer via its parametric outline. Fails (placeholder) with a
// reason for unmodeled shape kinds so the notice stays specific.
[[nodiscard]] std::optional<Layer> build_shape_layer(LayerBuildContext& ctx,
                                                     const af::AfClass& node,
                                                     const std::string& display,
                                                     std::string* why) {
  auto path = parametric_shape_path(node, why);
  if (environment_variable_is_set("PATCHY_AF_TRACE")) {
    const af::AfClass* shape = node.child_class(af::tag4("Shpe"));
    std::fprintf(stderr, "[af] shape '%s': kind=%08x path=%d why=%s\n", display.c_str(),
                 shape != nullptr ? shape->type_tag : 0U, path.has_value(),
                 why->c_str());
  }
  if (!path.has_value()) {
    return std::nullopt;
  }
  auto layer = build_vector_layer_from_path(ctx, node, display, std::move(*path));
  if (!layer.has_value()) {
    *why = "has no visible fill or stroke";
  }
  return layer;
}
// The node's own outline in document space (PCrv poly-curve or parametric
// ShpN), for use as a crop mask. Mirrors build_vector_layer_from_path's
// transform mapping; all subpaths share one Add group (the even-odd union
// rule the vector-mask adjunct import established).
[[nodiscard]] std::optional<VectorPath> crop_outline_path(LayerBuildContext& ctx,
                                                          const af::AfClass& node) {
  auto path = vector_path_from_node(node);
  if (!path.has_value() && node.type_tag == af::tag4("ShpN")) {
    std::string why;
    path = parametric_shape_path(node, &why);
  }
  if (!path.has_value() || path->subpaths.empty()) {
    return std::nullopt;
  }
  if (const auto xfrm = node_xfrm(ctx, node); xfrm.size() == 6) {
    transform_vector_path(*path,
                          {xfrm[0], xfrm[3], xfrm[1], xfrm[4], xfrm[2], xfrm[5]});
  }
  for (auto& subpath : path->subpaths) {
    subpath.shape_group = 0;
    subpath.op = PathCombineOp::Add;
  }
  return path;
}

}  // namespace patchy::af::detail
