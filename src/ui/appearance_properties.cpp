#include "ui/appearance_properties.hpp"
#include "core/vector_live_shapes.hpp"
#include "core/vector_compound.hpp"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

namespace patchy::ui {
namespace {
template <typename Settings, typename Getter>
void add_field(std::vector<AppearanceProperty<Settings>>& result, std::string key, Getter get) {
  using Value = std::remove_cv_t<std::remove_pointer_t<decltype(get(std::declval<const Settings&>()))>>;
  result.push_back(appearance_property<Settings, Value>(
      std::move(key), get, [get](Settings& target, const Value& value) {
        if (auto* field = get(target)) *field = value;
      }));
}

template <typename Settings, typename Getter>
void add_gradient(std::vector<AppearanceProperty<Settings>>& result, const std::string& key, Getter get) {
  add_field(result, key + ".definition", [get](auto& value) {
    using Gradient = std::conditional_t<std::is_const_v<std::remove_reference_t<decltype(value)>>,
                                         const GradientDefinition, GradientDefinition>;
    auto* gradient = get(value);
    return gradient ? static_cast<Gradient*>(gradient) : nullptr;
  });
#define GRADIENT_FIELD(member) add_field(result, key + "." #member, [get](auto& value) { auto* gradient = get(value); return gradient ? &gradient->member : nullptr; })
  GRADIENT_FIELD(type);
  GRADIENT_FIELD(angle_degrees);
  GRADIENT_FIELD(scale);
  GRADIENT_FIELD(reverse);
  GRADIENT_FIELD(dither);
  GRADIENT_FIELD(interpolation);
  GRADIENT_FIELD(align_with_layer);
  GRADIENT_FIELD(offset_x_percent);
  GRADIENT_FIELD(offset_y_percent);
#undef GRADIENT_FIELD
}

template <typename Getter>
void add_paint(std::vector<AppearanceProperty<ShapeAppearanceSettings>>& result, const std::string& key, Getter get) {
  using Settings = ShapeAppearanceSettings;
  auto kind = appearance_property<Settings, VectorFillKind>(
      key + ".kind", [get](const Settings& value) { return &get(value)->kind; },
      [get](Settings& value, VectorFillKind next) { get(value)->kind = next; });
  kind.capture = [get, key](const Settings& source) -> AppearanceEdit<Settings> {
    const auto paint = *get(source);
    return {key + ".kind", [get, paint](Settings& target) {
      auto& next = *get(target);
      if (next.kind != paint.kind) next = paint;
      else if (paint.kind == VectorFillKind::Solid) next.color = paint.color;
      else if (paint.kind == VectorFillKind::Gradient)
        static_cast<GradientDefinition&>(next.gradient) = paint.gradient;
      else if (paint.kind == VectorFillKind::Pattern) {
        next.pattern_id = paint.pattern_id;
        next.pattern_name = paint.pattern_name;
      }
    }};
  };
  result.push_back(std::move(kind));
  result.push_back(appearance_property<Settings, RgbColor>(
      key + ".color", [get](const Settings& value) { return &get(value)->color; },
      [get](Settings& target, RgbColor value) { get(target)->kind = VectorFillKind::Solid; get(target)->color = value; }));
  add_gradient(result, key + ".gradient", [get](auto& value) {
    auto* paint = get(value);
    return paint->kind == VectorFillKind::Gradient ? &paint->gradient : nullptr;
  });
#define PAINT_FIELD(member) add_field(result, key + "." #member, [get](auto& value) { auto* paint = get(value); return paint->kind == VectorFillKind::Pattern ? &paint->member : nullptr; })
  PAINT_FIELD(pattern_id);
  PAINT_FIELD(pattern_name);
  PAINT_FIELD(pattern_scale);
  PAINT_FIELD(pattern_angle_degrees);
  PAINT_FIELD(pattern_linked);
  PAINT_FIELD(pattern_phase_x);
  PAINT_FIELD(pattern_phase_y);
#undef PAINT_FIELD
}

template <auto Member>
void add_effects(std::vector<AppearanceProperty<LayerStyleSettings>>& result,
                 const LayerStyleSettings& reference, const std::string& prefix) {
  using Settings = LayerStyleSettings;
  using Vector = std::remove_cvref_t<decltype(reference.style.*Member)>;
  using Effect = typename Vector::value_type;
  const auto count = std::max<std::size_t>(1, (reference.style.*Member).size());
  for (std::size_t index = 0; index < count; ++index) {
    const auto key = prefix + "." + std::to_string(index);
    const auto get = [index](auto& value) {
      auto& effects = value.style.*Member;
      using Pointer = decltype(&effects.front());
      return index < effects.size() ? &effects[index] : Pointer{nullptr};
    };
    auto enabled = appearance_property<Settings, bool>(
        key + ".enabled", [get](const Settings& value) -> const bool* {
          auto* effect = get(value); return effect ? &effect->enabled : nullptr;
        }, [](Settings&, bool) {});
    enabled.capture = [get, index, key](const Settings& source) -> AppearanceEdit<Settings> {
      const auto* effect = get(source);
      const auto recipe = effect ? *effect : Effect{};
      return {key + ".enabled", [index, recipe](Settings& target) {
        auto& effects = target.style.*Member;
        if (index >= effects.size()) {
          if (!recipe.enabled) return;
          effects.resize(index + 1);
          effects[index] = recipe;
        } else {
          effects[index].enabled = recipe.enabled;
        }
      }};
    };
    result.push_back(std::move(enabled));
#define EFFECT_FIELD(member) if constexpr (requires(Effect e) { e.member; }) { add_field(result, key + "." #member, [get](auto& value) { auto* effect = get(value); return effect ? &effect->member : nullptr; }); }
    EFFECT_FIELD(blend_mode);
    EFFECT_FIELD(color);
    EFFECT_FIELD(opacity);
    EFFECT_FIELD(angle_degrees);
    EFFECT_FIELD(distance);
    EFFECT_FIELD(spread);
    EFFECT_FIELD(choke);
    EFFECT_FIELD(size);
    EFFECT_FIELD(layer_conceals);
    EFFECT_FIELD(use_global_light);
    EFFECT_FIELD(continuous);
    EFFECT_FIELD(fade);
    EFFECT_FIELD(technique);
    EFFECT_FIELD(range);
    EFFECT_FIELD(source);
    EFFECT_FIELD(position);
    EFFECT_FIELD(uses_gradient);
    EFFECT_FIELD(overprint);
    EFFECT_FIELD(highlight_blend_mode);
    EFFECT_FIELD(highlight_color);
    EFFECT_FIELD(highlight_opacity);
    EFFECT_FIELD(shadow_blend_mode);
    EFFECT_FIELD(shadow_color);
    EFFECT_FIELD(shadow_opacity);
    EFFECT_FIELD(altitude_degrees);
    EFFECT_FIELD(depth);
    EFFECT_FIELD(direction_up);
    EFFECT_FIELD(style);
    EFFECT_FIELD(soften);
    EFFECT_FIELD(gloss_contour);
    EFFECT_FIELD(gloss_anti_aliased);
    EFFECT_FIELD(invert);
    EFFECT_FIELD(unsupported_contour_options);
    EFFECT_FIELD(scale);
    EFFECT_FIELD(pattern_name);
    EFFECT_FIELD(pattern_id);
    EFFECT_FIELD(link_with_layer);
    EFFECT_FIELD(phase_x);
    EFFECT_FIELD(phase_y);
#undef EFFECT_FIELD
    if constexpr (requires(Effect e) { e.gradient; }) {
      add_gradient(result, key + ".gradient", [get](auto& value) {
        auto* effect = get(value);
        if constexpr (requires(Effect e) { e.uses_gradient; }) {
          if (effect && !effect->uses_gradient) return decltype(&effect->gradient){nullptr};
        }
        return effect ? &effect->gradient : nullptr;
      });
    }
    if constexpr (requires(Effect e) { e.contour; }) {
#define CONTOUR_FIELD(member) add_field(result, key + ".contour." #member, [get](auto& value) { auto* effect = get(value); return effect ? &effect->contour.member : nullptr; })
      CONTOUR_FIELD(enabled);
      result.back().capture = [get, index, key](const Settings& source) -> AppearanceEdit<Settings> {
        const auto* effect = get(source);
        const auto recipe = effect ? *effect : Effect{};
        return {key + ".contour.enabled", [index, recipe](Settings& target) {
          auto& effects = target.style.*Member;
          if (index >= effects.size()) {
            if (!recipe.contour.enabled) return;
            effects.resize(index + 1);
            effects[index] = recipe;
            effects[index].enabled = false;
            effects[index].texture.enabled = false;
          } else effects[index].contour.enabled = recipe.contour.enabled;
        }};
      };
      CONTOUR_FIELD(contour);
      CONTOUR_FIELD(anti_aliased);
      CONTOUR_FIELD(range);
#undef CONTOUR_FIELD
#define TEXTURE_FIELD(member) add_field(result, key + ".texture." #member, [get](auto& value) { auto* effect = get(value); return effect ? &effect->texture.member : nullptr; })
      TEXTURE_FIELD(enabled);
      result.back().capture = [get, index, key](const Settings& source) -> AppearanceEdit<Settings> {
        const auto* effect = get(source);
        const auto recipe = effect ? *effect : Effect{};
        return {key + ".texture.enabled", [index, recipe](Settings& target) {
          auto& effects = target.style.*Member;
          if (index >= effects.size()) {
            if (!recipe.texture.enabled) return;
            effects.resize(index + 1);
            effects[index] = recipe;
            effects[index].enabled = false;
            effects[index].contour.enabled = false;
          } else effects[index].texture.enabled = recipe.texture.enabled;
        }};
      };
      TEXTURE_FIELD(pattern_name);
      TEXTURE_FIELD(pattern_id);
      TEXTURE_FIELD(scale);
      TEXTURE_FIELD(depth);
      TEXTURE_FIELD(invert);
      TEXTURE_FIELD(link_with_layer);
      TEXTURE_FIELD(phase_x);
      TEXTURE_FIELD(phase_y);
#undef TEXTURE_FIELD
    }
  }
}
}  // namespace

ShapeAppearanceSettings shape_appearance_settings(const Layer& layer) {
  ShapeAppearanceSettings result;
  const auto* content = layer.vector_shape();
  if (!content) return result;
  result.fill = content->fill;
  result.stroke = content->stroke;
  result.layer_opacity = layer.opacity();
  result.fill_opacity = layer.fill_opacity();
  result.feather = content->feather;
  result.density = content->density;
  if (content->origination.size() == 1 && content->origination.front().raw_descriptor.empty() &&
      content->parts.empty()) {
    const auto& params = content->origination.front();
    if ((params.kind == LiveShapeKind::Rectangle || params.kind == LiveShapeKind::RoundedRectangle ||
         params.kind == LiveShapeKind::Ellipse || params.kind == LiveShapeKind::Line) &&
        std::all_of(content->path.subpaths.begin(), content->path.subpaths.end(),
                    [&](const auto& path) { return path.shape_group == params.index; }))
      result.geometry = params;
  }
  return result;
}

LayerStyleSettings layer_style_settings(const Layer& layer) {
  return {static_cast<int>(std::lround(layer.opacity() * 100.0F)), layer.blend_mode(),
          layer.layer_style(), layer.blend_if(), false,
          static_cast<int>(std::lround(layer.fill_opacity() * 100.0F)), layer.restricted_channels(),
          layer_knockout_setting(layer).value_or(0U)};
}

bool appearance_has_editable_radii(const ShapeAppearanceSettings& settings) {
  return settings.geometry && (settings.geometry->kind == LiveShapeKind::Rectangle ||
                               settings.geometry->kind == LiveShapeKind::RoundedRectangle);
}

std::vector<AppearanceProperty<ShapeAppearanceSettings>> shape_appearance_properties() {
  using Settings = ShapeAppearanceSettings;
  std::vector<AppearanceProperty<Settings>> result;
  add_paint(result, "fill", [](auto& value) { return &value.fill; });
  add_paint(result, "stroke.content", [](auto& value) { return &value.stroke.content; });
#define SHAPE_FIELD(member) add_field(result, #member, [](auto& value) { return &value.member; })
  SHAPE_FIELD(stroke.enabled);
  SHAPE_FIELD(stroke.fill_enabled);
  SHAPE_FIELD(stroke.width);
  SHAPE_FIELD(stroke.dash_offset);
  SHAPE_FIELD(stroke.miter_limit);
  SHAPE_FIELD(stroke.cap);
  SHAPE_FIELD(stroke.join);
  SHAPE_FIELD(stroke.alignment);
  SHAPE_FIELD(stroke.scale_lock);
  SHAPE_FIELD(stroke.stroke_adjust);
  SHAPE_FIELD(stroke.dashes);
  SHAPE_FIELD(stroke.blend_mode);
  SHAPE_FIELD(stroke.opacity);
  SHAPE_FIELD(stroke.resolution);
  SHAPE_FIELD(layer_opacity);
  SHAPE_FIELD(fill_opacity);
  SHAPE_FIELD(feather);
  SHAPE_FIELD(density);
#undef SHAPE_FIELD
  for (std::size_t corner = 0; corner < 4; ++corner) {
    result.push_back(appearance_property<Settings, double>(
        "radius." + std::to_string(corner),
        [corner](const Settings& value) -> const double* {
          return appearance_has_editable_radii(value) ? &value.geometry->corner_radii[corner] : nullptr;
        },
        [corner](Settings& target, double radius) {
          if (!appearance_has_editable_radii(target)) return;
          target.geometry->corner_radii[corner] = radius;
          if (radius > 0.0) target.geometry->kind = LiveShapeKind::RoundedRectangle;
        }));
  }
  auto geometry = appearance_property<Settings, std::optional<LiveShapeParams>>(
      "geometry", [](const Settings& value) { return &value.geometry; },
      [](Settings& target, const auto& value) { target.geometry = value; });
  geometry.equal = [](const Settings& a, const Settings& b) {
    auto left = a.geometry, right = b.geometry;
    for (auto* params : {&left, &right}) {
      if (*params) {
        (*params)->corner_radii = {};
        if ((*params)->kind == LiveShapeKind::RoundedRectangle) (*params)->kind = LiveShapeKind::Rectangle;
      }
    }
    return left == right;
  };
  result.push_back(std::move(geometry));
  return result;
}

std::vector<AppearanceProperty<LayerStyleSettings>> layer_style_properties(const LayerStyleSettings& reference) {
  using Settings = LayerStyleSettings;
  std::vector<AppearanceProperty<Settings>> result;
#define STYLE_FIELD(member) add_field(result, #member, [](auto& value) { return &value.member; })
  STYLE_FIELD(opacity);
  result.back().capture = [](const Settings& source) -> AppearanceEdit<Settings> {
    return {"opacity", [value = source.opacity](Settings& target) {
      target.opacity = value; target.opacity_edited = true;
    }};
  };
  STYLE_FIELD(fill_opacity);
  result.back().capture = [](const Settings& source) -> AppearanceEdit<Settings> {
    return {"fill_opacity", [value = source.fill_opacity](Settings& target) {
      target.fill_opacity = value; target.fill_opacity_edited = true;
    }};
  };
  STYLE_FIELD(blend_mode);
  STYLE_FIELD(replace_unsupported_blend_if);
  STYLE_FIELD(style.effects_visible);
  STYLE_FIELD(style.layer_mask_hides_effects);
  STYLE_FIELD(style.vector_mask_hides_effects);
  STYLE_FIELD(knockout);
  STYLE_FIELD(style.blend_interior_elements);
  STYLE_FIELD(style.blend_clipped_elements);
#undef STYLE_FIELD
  for (unsigned channel = 0; channel < 3; ++channel) {
    const auto key = "restricted_channels." + std::to_string(channel);
    const auto mask = static_cast<std::uint8_t>(1U << channel);
    result.push_back({key, [mask](const Settings& a, const Settings& b) {
      return (a.restricted_channels & mask) == (b.restricted_channels & mask);
    }, [mask, key](const Settings& source) -> AppearanceEdit<Settings> {
      const auto value = source.restricted_channels & mask;
      return {key, [mask, value](Settings& target) {
        target.restricted_channels = static_cast<std::uint8_t>((target.restricted_channels & ~mask) | value);
      }};
    }});
  }
  for (std::size_t channel = 0; channel < reference.blend_if.channels.size(); ++channel) {
#define BLEND_FIELD(side, member) add_field(result, "blend_if." + std::to_string(channel) + "." #side "." #member, [channel](auto& value) { return &value.blend_if.channels[channel].side.member; })
    BLEND_FIELD(this_layer, black_low);
    BLEND_FIELD(this_layer, black_high);
    BLEND_FIELD(this_layer, white_low);
    BLEND_FIELD(this_layer, white_high);
    BLEND_FIELD(underlying_layer, black_low);
    BLEND_FIELD(underlying_layer, black_high);
    BLEND_FIELD(underlying_layer, white_low);
    BLEND_FIELD(underlying_layer, white_high);
#undef BLEND_FIELD
  }
  add_effects<&LayerStyle::drop_shadows>(result, reference, "drop_shadows");
  add_effects<&LayerStyle::inner_shadows>(result, reference, "inner_shadows");
  add_effects<&LayerStyle::outer_glows>(result, reference, "outer_glows");
  add_effects<&LayerStyle::inner_glows>(result, reference, "inner_glows");
  add_effects<&LayerStyle::color_overlays>(result, reference, "color_overlays");
  add_effects<&LayerStyle::gradient_fills>(result, reference, "gradient_fills");
  add_effects<&LayerStyle::pattern_overlays>(result, reference, "pattern_overlays");
  add_effects<&LayerStyle::strokes>(result, reference, "strokes");
  add_effects<&LayerStyle::bevels>(result, reference, "bevels");
  add_effects<&LayerStyle::satins>(result, reference, "satins");
  return result;
}

bool shape_appearance_equal(const ShapeAppearanceSettings& a, const ShapeAppearanceSettings& b) {
  return a.fill == b.fill && a.stroke == b.stroke && a.geometry == b.geometry &&
         a.layer_opacity == b.layer_opacity && a.fill_opacity == b.fill_opacity &&
         a.feather == b.feather && a.density == b.density;
}

bool layer_style_settings_equal(const LayerStyleSettings& a, const LayerStyleSettings& b) {
  // Type-specific parameter patches skip solid strokes, but full replacement
  // still carries their stored gradient recipes, including disabled effects.
  if (a.style.strokes.size() != b.style.strokes.size()) return false;
  for (std::size_t i = 0; i < a.style.strokes.size(); ++i)
    if (a.style.strokes[i].gradient != b.style.strokes[i].gradient) return false;
  const auto left = layer_style_properties(a);
  const auto right = layer_style_properties(b);
  return std::all_of(left.begin(), left.end(), [&](const auto& field) { return field.equal(a, b); }) &&
         std::all_of(right.begin(), right.end(), [&](const auto& field) { return field.equal(a, b); });
}

bool shape_vector_appearance_equal(const VectorShapeContent& a, const VectorShapeContent& b) {
  if (a.fill != b.fill || a.stroke != b.stroke || a.path != b.path ||
      a.origination != b.origination || a.feather != b.feather || a.density != b.density ||
      a.parts.size() != b.parts.size()) return false;
  for (std::size_t i = 0; i < a.parts.size(); ++i)
    if (a.parts[i].fill != b.parts[i].fill || a.parts[i].stroke != b.parts[i].stroke) return false;
  return true;
}

ShapeAppearanceSettings apply_shape_appearance_edits(const ShapeAppearanceSettings& original,
                                                     const ShapeAppearanceSettings& edited) {
  if (!edited.preview_enabled) return original;
  auto result = edited.edits ? edited.edits->applied(original) : edited;
  result.edits = edited.edits;
  // Preview belongs to the transaction, not to an Apply All recipe captured
  // while preview was off. Compound parts replay this same edit log below.
  result.preview_enabled = edited.preview_enabled;
  return result;
}

LayerStyleSettings apply_layer_style_edits(const LayerStyleSettings& original,
                                           const LayerStyleSettings& edited) {
  if (!edited.preview_enabled) return original;
  return edited.edits ? edited.edits->applied(original) : edited;
}

VectorShapeContent assemble_shape_appearance(const VectorShapeContent& original,
                                             const ShapeAppearanceSettings& settings) {
  auto content = original;
  content.fill = settings.fill;
  content.stroke = settings.stroke;
  content.feather = settings.feather;
  content.density = settings.density;
  if (!settings.edits) update_vector_part_appearance(content, original.fill, original.stroke);
  else for (auto& part : content.parts) {
    ShapeAppearanceSettings baseline;
    baseline.fill = part.fill;
    baseline.stroke = part.stroke;
    const auto next = apply_shape_appearance_edits(baseline, settings);
    part.fill = next.fill;
    part.stroke = next.stroke;
  }
  if (settings.geometry && content.origination.size() == 1 &&
      *settings.geometry != content.origination.front()) {
    auto params = *settings.geometry;
    params.index = content.origination.front().index;
    const auto& prior = content.origination.front();
    if (params.left != prior.left || params.top != prior.top ||
        params.right != prior.right || params.bottom != prior.bottom)
      populate_live_shape_box_corners(params);
    auto paths = generate_live_shape_subpaths(params);
    if (params.kind == LiveShapeKind::Line) {
      VectorPath path;
      path.subpaths = paths;
      if (const auto bounds = path.bounds()) {
        params.left = bounds->left; params.top = bounds->top;
        params.right = bounds->right; params.bottom = bounds->bottom;
      }
    }
    auto op = PathCombineOp::Add;
    for (const auto& path : content.path.subpaths)
      if (path.shape_group == params.index) { op = path.op; break; }
    std::erase_if(content.path.subpaths, [&](const auto& path) { return path.shape_group == params.index; });
    for (auto& path : paths) {
      path.shape_group = params.index; path.op = op;
      content.path.subpaths.push_back(std::move(path));
    }
    content.origination.front() = params;
  }
  return content;
}
}  // namespace patchy::ui
