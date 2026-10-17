// Artboard scripting (docs/artboards.md): doc.artboards, doc.addArtboard,
// doc.artboardFromLayers, doc.fitCanvasToArtboards, doc.exportArtboards, layer.isArtboard,
// layer.artboard and layer.setArtboard. Every name and token here is permanent.
#include "core/artboard.hpp"
#include "core/layer_render_utils.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/script_api.hpp"
#include "ui/script_vector.hpp"

#include <QColor>
#include <QJSValueIterator>
#include <QDir>

#include <algorithm>
#include <optional>

namespace patchy::ui {
using namespace script_vector;
namespace {

template <class F>
auto artboard_guarded(ScriptEngineHost& host, F&& function) -> std::invoke_result_t<F> {
  const ScriptApiCall api_call(host);
  using Result = std::invoke_result_t<F>;
  try {
    return function();
  } catch (const std::exception& error) {
    host.throw_js_error(
        ScriptEngineHost::tr("Invalid artboard option or target: %1.").arg(QString::fromUtf8(error.what())));
    if constexpr (!std::is_void_v<Result>) {
      return Result{};
    }
  }
}

QString background_id(ArtboardBackground background) {
  switch (background) {
    case ArtboardBackground::Black:
      return QStringLiteral("black");
    case ArtboardBackground::Transparent:
      return QStringLiteral("transparent");
    case ArtboardBackground::Custom:
      return QStringLiteral("color");
    case ArtboardBackground::White:
      break;
  }
  return QStringLiteral("white");
}

QJsonObject artboard_json(const LayerArtboard& artboard) {
  return {{"x", artboard.rect.x},
          {"y", artboard.rect.y},
          {"width", artboard.rect.width},
          {"height", artboard.rect.height},
          {"background", background_id(artboard.background)},
          {"color", QColor(artboard.color.red, artboard.color.green, artboard.color.blue).name()}};
}

// Applies the rect and background keys of `options` to `artboard`; absent keys keep it.
// "color" without "background" means a custom color.
void apply_artboard_options(const QJsonObject& options, LayerArtboard& artboard) {
  keys(options, {"x", "y", "width", "height", "background", "color"});
  artboard.rect.x = integer(options, "x", artboard.rect.x, -30000, 30000);
  artboard.rect.y = integer(options, "y", artboard.rect.y, -30000, 30000);
  artboard.rect.width = integer(options, "width", artboard.rect.width, 1, 30000);
  artboard.rect.height = integer(options, "height", artboard.rect.height, 1, 30000);
  if (options.contains("color")) {
    const QColor color(string(options, "color"));
    if (!color.isValid()) {
      invalid("color");
    }
    artboard.color = RgbColor{static_cast<std::uint8_t>(color.red()), static_cast<std::uint8_t>(color.green()),
                              static_cast<std::uint8_t>(color.blue())};
    artboard.background = ArtboardBackground::Custom;
  }
  if (options.contains("background")) {
    const auto token = string(options, "background");
    if (token == QStringLiteral("white")) {
      artboard.background = ArtboardBackground::White;
    } else if (token == QStringLiteral("black")) {
      artboard.background = ArtboardBackground::Black;
    } else if (token == QStringLiteral("transparent")) {
      artboard.background = ArtboardBackground::Transparent;
    } else if (token == QStringLiteral("color")) {
      artboard.background = ArtboardBackground::Custom;
    } else {
      invalid("background");
    }
  }
  if (artboard.background != ArtboardBackground::Custom) {
    artboard.color = RgbColor{255, 255, 255};
  }
}

Layer* top_level_layer(Document& document, LayerId id) {
  for (auto& layer : document.layers()) {
    if (layer.id() == id) {
      return &layer;
    }
  }
  return nullptr;
}

// Commits an edited copy of the document: the Artboard tool's checks first (the canvas may
// not pass kMaxArtboardCanvasSide, nor change under Smart Filters), so a refusal leaves
// the document untouched; then the canvas grows to hold every artboard.
void commit_artboard_change(ScriptEngineHost& host, std::int64_t session, Document staged) {
  if (!artboard_canvas_within_limit(staged)) {
    invalid(QStringLiteral("the canvas would pass %1 px").arg(kMaxArtboardCanvasSide));
  }
  if (artboard_canvas_would_grow(staged) &&
      (document_contains_smart_filters(staged) || document_contains_unparsed_smart_objects(staged))) {
    invalid(QStringLiteral("the canvas cannot grow while the document has Smart Filters"));
  }
  if (!std::as_const(staged).metadata().artboard_settings.has_value() &&
      document_has_artboards(std::as_const(staged))) {
    staged.metadata().artboard_settings.emplace();
  }
  static_cast<void>(grow_canvas_to_artboards(staged));
  auto& document = writable(host, session);
  document = std::move(staged);
  host.note_structure_changed(session);
}

}  // namespace

Rect script_layer_position_bounds(const Layer& layer) {
  if (const auto& artboard = layer.artboard(); artboard.has_value()) {
    return artboard->rect;
  }
  return layer.bounds();
}

bool ScriptLayerObject::is_artboard() const {
  return artboard_guarded(host_, [&] { return layer_is_artboard(layer(host_, session_id_, layer_id_)); });
}

QJSValue ScriptLayerObject::artboard() const {
  return artboard_guarded(host_, [&] {
    const auto& item = layer(host_, session_id_, layer_id_);
    if (!item.artboard().has_value()) {
      return QJSValue(QJSValue::NullValue);
    }
    return to_js(host_, artboard_json(*item.artboard()));
  });
}

void ScriptLayerObject::setArtboard(const QJSValue& options) {
  artboard_guarded(host_, [&] {
    const auto& current = layer(host_, session_id_, layer_id_, true);
    const auto& original = document(host_, session_id_);
    const bool top_level = std::any_of(original.layers().begin(), original.layers().end(),
                                       [&](const Layer& item) { return item.id() == layer_id_; });
    if (options.isNull()) {
      // Back to a plain group (Layer > New > Convert Artboard to Group).
      if (!current.artboard().has_value()) {
        return;
      }
      auto& doc = writable(host_, session_id_);
      auto* target = top_level_layer(doc, layer_id_);
      if (target == nullptr) {
        invalid("layer.topLevel");
      }
      target->clear_artboard();
      host_.note_structure_changed(session_id_);
      return;
    }
    const auto parsed = object(options);
    LayerArtboard changed;
    if (current.artboard().has_value()) {
      changed = *current.artboard();
    } else {
      // A new artboard from a top-level group: its content's bounds unless the call says.
      if (current.kind() != LayerKind::Group || !top_level) {
        invalid("layer.group");
      }
      if (std::any_of(current.children().begin(), current.children().end(),
                      [](const Layer& child) { return layer_is_artboard(child); })) {
        invalid("layer.group");
      }
      changed.rect = layer_render_bounds(current);
      if (changed.rect.empty()) {
        changed.rect = Rect::from_size(original.width(), original.height());
      }
    }
    apply_artboard_options(parsed, changed);
    if (current.artboard().has_value() && *current.artboard() == changed) {
      return;
    }
    if (!top_level) {
      invalid("layer.topLevel");
    }
    auto staged = original;
    auto* target = top_level_layer(staged, layer_id_);
    if (target == nullptr) {
      invalid("layer.topLevel");
    }
    if (!std::as_const(*target).artboard().has_value() && target->blend_mode() == BlendMode::PassThrough) {
      target->set_blend_mode(BlendMode::Normal);  // artboards are isolated, like Photoshop's
    }
    target->set_artboard(std::move(changed));
    commit_artboard_change(host_, session_id_, std::move(staged));
  });
}

QJSValue ScriptDocumentObject::artboards() const {
  return artboard_guarded(host_, [&] {
    auto array = host_.engine()->newArray();
    quint32 index = 0;
    for (const auto* artboard : document_artboards(document(host_, session_id_))) {
      array.setProperty(index++, make_layer_value(host_, session_id_, artboard->id()));
    }
    return array;
  });
}

QJSValue ScriptDocumentObject::addArtboard(const QString& name, const QJSValue& rect, const QJSValue& options) {
  return artboard_guarded(host_, [&] {
    const auto& original = document(host_, session_id_);
    const auto geometry = object(rect);
    keys(geometry, {"x", "y", "width", "height"});
    for (const auto* key : {"x", "y", "width", "height"}) {
      if (!geometry.contains(QLatin1String(key))) {
        invalid(QStringLiteral("rect.%1").arg(QLatin1String(key)));
      }
    }
    LayerArtboard artboard;
    artboard.rect = Rect{integer(geometry, "x", 0, -30000, 30000), integer(geometry, "y", 0, -30000, 30000),
                         integer(geometry, "width", 1, 1, 30000), integer(geometry, "height", 1, 1, 30000)};
    if (!options.isUndefined() && !options.isNull()) {
      auto extra = object(options);
      keys(extra, {"background", "color"});
      apply_artboard_options(extra, artboard);
    }
    const auto artboard_name =
        name.trimmed().isEmpty() ? next_artboard_name(original) : name.toStdString();
    auto staged = original;
    auto group = make_artboard_group(staged.allocate_layer_id(), artboard_name, artboard.rect, artboard.background,
                                     artboard.color);
    const auto id = group.id();
    staged.layers().insert(staged.layers().begin() + static_cast<std::ptrdiff_t>(new_artboard_insert_index(staged)),
                           std::move(group));
    staged.set_active_layer(id);
    commit_artboard_change(host_, session_id_, std::move(staged));
    return make_layer_value(host_, session_id_, id);
  });
}

QJSValue ScriptDocumentObject::artboardFromLayers(const QJSValue& layers, const QString& name) {
  return artboard_guarded(host_, [&] {
    if (!layers.isArray() || layers.property(QStringLiteral("length")).toUInt() > 10000) {
      invalid("layers");
    }
    std::vector<LayerId> ids;
    const auto count = layers.property(QStringLiteral("length")).toUInt();
    for (quint32 i = 0; i < count; ++i) {
      const auto* wrapper = qobject_cast<ScriptLayerObject*>(layers.property(i).toQObject());
      if (wrapper == nullptr || wrapper->session_id() != session_id_) {
        invalid("layers");
      }
      (void)layer(host_, session_id_, wrapper->layer_id(), true);
      ids.push_back(wrapper->layer_id());
    }
    // Validate on a copy first so a refusal leaves the document (and undo) untouched.
    auto staged = document(host_, session_id_);
    const auto id = put_layers_on_new_artboard(staged, ids, name.trimmed().toStdString());
    if (!id.has_value()) {
      invalid("layers");
    }
    staged.set_active_layer(*id);
    commit_artboard_change(host_, session_id_, std::move(staged));
    return make_layer_value(host_, session_id_, *id);
  });
}

QJSValue ScriptDocumentObject::fitCanvasToArtboards() {
  return artboard_guarded(host_, [&] {
    const auto& original = document(host_, session_id_);
    QJsonObject shift{{"x", 0}, {"y", 0}};
    const auto extent = artboard_document_extent(original);
    if (!extent.has_value() || *extent == Rect::from_size(original.width(), original.height())) {
      return to_js(host_, shift);
    }
    auto& doc = writable(host_, session_id_);
    if (const auto applied = fit_canvas_to_artboards(doc); applied.has_value()) {
      shift = QJsonObject{{"x", (*applied)[0]}, {"y", (*applied)[1]}};
    }
    host_.note_structure_changed(session_id_);
    return to_js(host_, shift);
  });
}

QJSValue ScriptDocumentObject::exportArtboards(const QString& folder, const QJSValue& options) {
  return artboard_guarded(host_, [&] {
    const auto& source = document(host_, session_id_);
    if (folder.trimmed().isEmpty()) {
      invalid("folder");
    }
    // `artboards` holds layer wrappers, which have no JSON form: the other keys are
    // read through a copy without it.
    QJsonObject parsed;
    if (!options.isUndefined() && !options.isNull()) {
      if (!options.isObject() || options.isArray()) {
        invalid("options");
      }
      auto plain = host_.engine()->newObject();
      QJSValueIterator it(options);
      while (it.hasNext()) {
        it.next();
        if (it.name() != QStringLiteral("artboards")) {
          plain.setProperty(it.name(), it.value());
        }
      }
      parsed = object(plain);
    }
    keys(parsed, {"format", "prefix", "overwrite"});
    const auto extension = string(parsed, "format", QStringLiteral("png")).toLower();
    std::vector<LayerId> ids;
    if (const auto chosen = options.isObject() ? options.property(QStringLiteral("artboards")) : QJSValue();
        !chosen.isUndefined()) {
      if (!chosen.isArray()) {
        invalid("artboards");
      }
      for (quint32 i = 0; i < chosen.property(QStringLiteral("length")).toUInt(); ++i) {
        const auto* wrapper = qobject_cast<ScriptLayerObject*>(chosen.property(i).toQObject());
        if (wrapper == nullptr || wrapper->session_id() != session_id_ ||
            !layer_is_artboard(layer(host_, session_id_, wrapper->layer_id()))) {
          invalid("artboards");
        }
        ids.push_back(wrapper->layer_id());
      }
    } else {
      for (const auto* artboard : document_artboards(source)) {
        ids.push_back(artboard->id());
      }
    }
    if (ids.empty()) {
      invalid("artboards.length");
    }
    const std::optional<QString> prefix =
        parsed.contains("prefix") ? std::optional<QString>(string(parsed, "prefix")) : std::nullopt;
    QString error;
    const auto written = host_.export_artboards(session_id_, ids, QDir::fromNativeSeparators(folder), extension,
                                                prefix, boolean(parsed, "overwrite", false), &error);
    if (!written.has_value()) {
      host_.throw_js_error(error.isEmpty() ? ScriptEngineHost::tr("The artboards could not be exported.") : error);
      return QJSValue();
    }
    auto array = host_.engine()->newArray();
    quint32 index = 0;
    for (const auto& path : *written) {
      array.setProperty(index++, QDir::fromNativeSeparators(path));
    }
    return array;
  });
}

}  // namespace patchy::ui
