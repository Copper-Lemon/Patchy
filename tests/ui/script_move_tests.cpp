#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include "ui_test_access.hpp"
// Script layer moves (layer.moveTo and the x / y setters) carry the placement
// data the Move tool carries: the shape model, smart-object quads, the text
// transform, and a linked mask (docs/scripting.md). Before September 30, 2026
// they shifted the pixels alone, so a shape or smart object snapped back at its
// next re-render and Photoshop re-laid text out at the creation anchor.
#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/layer_metadata.hpp"
#include "ui/main_window.hpp"
#include "ui/script_engine.hpp"
#include "test_harness.hpp"
#include "ui/ui_test_access.hpp"
#include "ui_test_support.hpp"
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace patchy::test::ui;
using patchy::ui::MainWindow;
using patchy::ui::MainWindowTestAccess;
using patchy::ui::ScriptEngineHost;

// Runs to completion, prints the console on a failure, and returns patchy.setResult's value.
QJsonValue run(MainWindow& window, const QString& code) {
  auto& host = window.script_engine_host();
  ScriptEngineHost::RunOptions options;
  options.name = QStringLiteral("move-test");
  options.unattended = true;
  const QString helpers = QStringLiteral(R"JS(
    function check(ok, message) { if (!ok) throw Error(message || 'check failed'); }
    function near(actual, expected, what) {
      if (Math.abs(actual - expected) > 0.01) throw Error(what + ': ' + actual + ' != ' + expected);
    }
    function pathOrigin(path) {
      var x = Infinity, y = Infinity;
      path.subpaths.forEach(function (sub) {
        sub.anchors.forEach(function (a) { x = Math.min(x, a.x); y = Math.min(y, a.y); });
      });
      return {x: x, y: y};
    }
  )JS");
  const bool started = host.run_source(helpers + code, options);
  CHECK(started);
  QElapsedTimer deadline;
  deadline.start();
  while (host.run_active() && deadline.elapsed() < 30000) {
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  QApplication::processEvents();
  CHECK(!host.run_active());
  if (!started || host.last_run_had_error()) {
    for (const auto& text : host.message_backlog()) {
      std::fprintf(stderr, "%s\n", text.toUtf8().constData());
    }
  }
  CHECK(!host.last_run_had_error());
  return host.last_result();
}

QString literal(const QString& text) {
  const auto json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
  return QString::fromUtf8(json.mid(1, json.size() - 2));
}

QString artifact_path(const QString& name) {
  ensure_artifact_dir();
  const auto dir = QFileInfo(QStringLiteral("test-artifacts/script-move")).absoluteFilePath();
  CHECK(QDir().mkpath(dir));
  const auto path = dir + QLatin1Char('/') + name;
  QFile::remove(path);
  return path;
}

const patchy::Layer* layer_with_id(MainWindow& window, const QJsonValue& id) {
  const auto* layer =
      std::as_const(MainWindowTestAccess::document(window)).find_layer(id.toString().toULongLong());
  CHECK(layer != nullptr);
  return layer;
}

std::optional<patchy::LayerAffineTransform> text_transform(const patchy::Layer& layer) {
  const auto found = layer.metadata().find(patchy::kLayerMetadataTextTransform);
  if (found == layer.metadata().end()) {
    return std::nullopt;
  }
  return patchy::parse_layer_affine_transform(found->second);
}

bool rect_equals(const patchy::Rect& rect, int x, int y, int width, int height) {
  return rect.x == x && rect.y == y && rect.width == width && rect.height == height;
}

// An opaque raster mask over `bounds`, linked or not. The scripting API has no raster
// mask surface, so the layer is dressed here between two script runs.
void give_raster_mask(MainWindow& window, const QJsonValue& id, patchy::Rect bounds, bool linked) {
  auto* layer = MainWindowTestAccess::document(window).find_layer(id.toString().toULongLong());
  CHECK(layer != nullptr);
  if (layer == nullptr) {
    return;
  }
  patchy::LayerMask mask;
  mask.bounds = bounds;
  mask.pixels = patchy::PixelBuffer(bounds.width, bounds.height, patchy::PixelFormat::gray8());
  mask.pixels.clear(255);
  layer->set_mask(std::move(mask));
  patchy::set_layer_mask_linked(*layer, linked);
}

// moveTo and both setters move the shape model with the raster, a re-render keeps the
// layer where the script put it, and the saved PSD reopens there.
void ui_script_move_carries_shape_geometry() {
  MainWindow window;
  show_window(window);
  window.set_cli_automation_mode(true);
  const auto psd = artifact_path(QStringLiteral("shape.psd"));
  run(window, QStringLiteral(R"JS(
    var psd = %1;
    var d = app.newDocument(400, 300);
    var box = d.addShape('Box', {type: 'rectangle', x: 10, y: 10, width: 50, height: 40});
    var g0 = box.getShape().liveShapes[0].geometry, b0 = box.bounds;
    function expectAt(layer, dx, dy, what) {
      var g = layer.getShape().liveShapes[0].geometry, b = layer.bounds;
      check(b.x === b0.x + dx && b.y === b0.y + dy && b.width === b0.width && b.height === b0.height,
            what + ': bounds ' + JSON.stringify(b));
      near(g.x, g0.x + dx, what + ' geometry x'); near(g.y, g0.y + dy, what + ' geometry y');
      near(g.width, g0.width, what + ' geometry width'); near(g.height, g0.height, what + ' geometry height');
      var origin = pathOrigin(layer.getShape().path);
      near(origin.x, 10 + dx, what + ' path x'); near(origin.y, 10 + dy, what + ' path y');
    }
    expectAt(box, 0, 0, 'created');
    box.moveTo(b0.x + 190, b0.y + 140);
    expectAt(box, 190, 140, 'moveTo');
    box.x = b0.x + 110;
    expectAt(box, 110, 140, 'x setter');
    box.y = b0.y + 80;
    expectAt(box, 110, 80, 'y setter');
    // An appearance change re-renders from the model: the layer must not snap back.
    box.updateShape({fill: '#3366ff'});
    expectAt(box, 110, 80, 're-rendered');
    check(d.saveAs(psd), 'save');
    d.close();
    var re = app.open(psd);
    expectAt(re.findLayer('Box'), 110, 80, 'reopened');
    re.close();
  )JS")
                  .arg(literal(psd)));
  CHECK(QFileInfo::exists(psd));
}

// The text transform is the anchor Photoshop lays the text out from. It follows the
// move, survives the PSD round trip, and a retype stays at the new place.
void ui_script_move_carries_text_anchor() {
  MainWindow window;
  show_window(window);
  window.set_cli_automation_mode(true);
  const auto unmoved_psd = artifact_path(QStringLiteral("text-unmoved.psd"));
  const auto psd = artifact_path(QStringLiteral("text.psd"));
  const auto id = run(window, QStringLiteral(R"JS(
    var d = app.newDocument(600, 400);
    var t = d.addTextLayer('Patchy', {size: 24, x: 20, y: 300});
    check(d.saveAs(%1), 'save the unmoved layer');
    patchy.setResult(t.id);
  )JS")
                                  .arg(literal(unmoved_psd)));
  const auto* created = layer_with_id(window, id);
  const auto created_bounds = created->bounds();
  const auto created_transform = text_transform(*created);
  CHECK(created_transform.has_value());
  if (!created_transform.has_value()) {
    return;
  }

  run(window, QStringLiteral("var t = app.activeDocument.getLayer(%1); t.moveTo(t.x + 150, t.y - 120);")
                  .arg(literal(id.toString())));
  const auto* moved = layer_with_id(window, id);
  const auto moved_bounds = moved->bounds();
  CHECK(rect_equals(moved_bounds, created_bounds.x + 150, created_bounds.y - 120, created_bounds.width,
                    created_bounds.height));
  const auto moved_transform = text_transform(*moved);
  CHECK(moved_transform.has_value());
  if (!moved_transform.has_value()) {
    return;
  }
  for (std::size_t i = 0; i < 4U; ++i) {
    CHECK((*moved_transform)[i] == (*created_transform)[i]);
  }
  CHECK(std::abs((*moved_transform)[4] - ((*created_transform)[4] + 150.0)) < 1e-9);
  CHECK(std::abs((*moved_transform)[5] - ((*created_transform)[5] - 120.0)) < 1e-9);

  // The PSD stores the anchor in Photoshop's form (a point-text baseline), so the saved
  // files are compared with each other: the moved one differs by exactly the move.
  run(window, QStringLiteral("var d = app.activeDocument; check(d.saveAs(%1), 'save'); d.close();").arg(literal(psd)));
  const auto reopen_text = [&window](const QString& path) {
    return run(window, QStringLiteral(R"JS(
      var re = app.open(%1);
      var texts = re.layers.filter(function (layer) { return layer.isText; });
      check(texts.length === 1, 'text layers after reopen: ' + texts.length);
      patchy.setResult(texts[0].id);
    )JS")
                           .arg(literal(path)));
  };
  const auto unmoved_id = reopen_text(unmoved_psd);
  const auto* unmoved = layer_with_id(window, unmoved_id);
  CHECK(rect_equals(unmoved->bounds(), created_bounds.x, created_bounds.y, created_bounds.width,
                    created_bounds.height));
  const auto unmoved_transform = text_transform(*unmoved);
  CHECK(unmoved_transform.has_value());
  run(window, QStringLiteral("app.activeDocument.close();"));

  const auto reopened_id = reopen_text(psd);
  const auto* reopened = layer_with_id(window, reopened_id);
  CHECK(rect_equals(reopened->bounds(), moved_bounds.x, moved_bounds.y, moved_bounds.width, moved_bounds.height));
  const auto reopened_transform = text_transform(*reopened);
  CHECK(reopened_transform.has_value());
  if (reopened_transform.has_value() && unmoved_transform.has_value()) {
    for (std::size_t i = 0; i < 4U; ++i) {
      CHECK(std::abs((*reopened_transform)[i] - (*unmoved_transform)[i]) < 1e-9);
    }
    CHECK(std::abs((*reopened_transform)[4] - ((*unmoved_transform)[4] + 150.0)) < 1e-6);
    CHECK(std::abs((*reopened_transform)[5] - ((*unmoved_transform)[5] - 120.0)) < 1e-6);
  }

  // A retype lays the text out again from the anchor.
  run(window, QStringLiteral("app.activeDocument.getLayer(%1).text = 'Patchx';").arg(literal(reopened_id.toString())));
  const auto retyped_bounds = layer_with_id(window, reopened_id)->bounds();
  CHECK(std::abs(retyped_bounds.x - moved_bounds.x) <= 3);
  CHECK(std::abs(retyped_bounds.y - moved_bounds.y) <= 3);
}

// A linked mask rides the move exactly once; an unlinked mask stays where it is, for
// raster and vector masks alike (the Move tool's rule).
void ui_script_move_follows_the_mask_link_rule() {
  MainWindow window;
  show_window(window);
  const auto ids = run(window, QStringLiteral(R"JS(
    var d = app.newDocument(200, 200);
    var path = {subpaths: [{closed: true, anchors: [{x: 20, y: 30}, {x: 60, y: 30}, {x: 60, y: 70}, {x: 20, y: 70}]}]};
    var ids = {};
    ['Linked', 'Unlinked', 'Vector linked', 'Vector unlinked'].forEach(function (name) {
      var layer = d.addLayer(name);
      layer.fillRect(20, 30, 40, 40, '#ff4000');
      ids[name] = layer.id;
    });
    d.findLayer('Vector linked').setVectorMask({path: path});
    d.findLayer('Vector unlinked').setVectorMask({path: path, linked: false});
    patchy.setResult(ids);
  )JS")).toObject();
  const patchy::Rect mask_bounds{20, 30, 40, 40};
  give_raster_mask(window, ids["Linked"], mask_bounds, true);
  give_raster_mask(window, ids["Unlinked"], mask_bounds, false);
  const auto undo_before = MainWindowTestAccess::active_session_undo_depth(window);

  run(window, QStringLiteral(R"JS(
    var d = app.activeDocument;
    ['Linked', 'Unlinked', 'Vector linked', 'Vector unlinked'].forEach(function (name) {
      var layer = d.findLayer(name);
      layer.moveTo(layer.x + 30, layer.y + 10);
      check(layer.bounds.x === 50 && layer.bounds.y === 40, name + ' bounds ' + JSON.stringify(layer.bounds));
    });
    var linked = pathOrigin(d.findLayer('Vector linked').getVectorMask().path);
    check(linked.x === 50 && linked.y === 40, 'linked vector mask ' + JSON.stringify(linked));
    var unlinked = d.findLayer('Vector unlinked').getVectorMask();
    check(!unlinked.linked, 'unlinked flag');
    var stayed = pathOrigin(unlinked.path);
    check(stayed.x === 20 && stayed.y === 30, 'unlinked vector mask ' + JSON.stringify(stayed));
  )JS"));
  const auto* linked = layer_with_id(window, ids["Linked"]);
  CHECK(linked->mask().has_value() && rect_equals(linked->mask()->bounds, 50, 40, 40, 40));
  const auto* unlinked = layer_with_id(window, ids["Unlinked"]);
  CHECK(rect_equals(unlinked->bounds(), 50, 40, 40, 40));
  CHECK(unlinked->mask().has_value() && rect_equals(unlinked->mask()->bounds, 20, 30, 40, 40));
  CHECK(!patchy::layer_mask_linked(*unlinked));

  // The whole run is one undo step, and it puts the masks back with the layers.
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == undo_before + 1);
  MainWindowTestAccess::undo(window);
  QApplication::processEvents();
  const auto* undone = layer_with_id(window, ids["Linked"]);
  CHECK(rect_equals(undone->bounds(), 20, 30, 40, 40));
  CHECK(undone->mask().has_value() && rect_equals(undone->mask()->bounds, 20, 30, 40, 40));
}

// Moving a group moves every descendant's placement data, nested groups included.
void ui_script_move_group_carries_children() {
  MainWindow window;
  show_window(window);
  const auto ids = run(window, QStringLiteral(R"JS(
    var d = app.newDocument(400, 300);
    var shape = d.addShape('Box', {type: 'rectangle', x: 10, y: 10, width: 50, height: 40});
    var text = d.addTextLayer('Patchy', {size: 24, x: 20, y: 120});
    var pixels = d.addLayer('Pixels');
    pixels.fillRect(100, 100, 30, 30, '#ff4000');
    var inner = d.groupLayers([pixels], 'Inner');
    var outer = d.groupLayers([shape, text, inner], 'Outer');
    check(outer.isGroup && outer.children.length === 3, 'group shape');
    patchy.setResult({shape: shape.id, text: text.id, pixels: pixels.id, outer: outer.id});
  )JS")).toObject();
  give_raster_mask(window, ids["pixels"], patchy::Rect{100, 100, 30, 30}, true);
  const auto text_before = text_transform(*layer_with_id(window, ids["text"]));
  const auto text_bounds_before = layer_with_id(window, ids["text"])->bounds();
  CHECK(text_before.has_value());
  if (!text_before.has_value()) {
    return;
  }

  run(window, QStringLiteral(R"JS(
    var d = app.activeDocument, ids = %1;
    var shape = d.getLayer(ids.shape), outer = d.getLayer(ids.outer);
    var g0 = shape.getShape().liveShapes[0].geometry, b0 = shape.bounds;
    // A group has no position of its own, so moveTo offsets its contents.
    check(outer.x === 0 && outer.y === 0, 'group position ' + outer.x + ',' + outer.y);
    outer.moveTo(15, 25);
    check(outer.x === 0 && outer.y === 0, 'group position after the move');
    var g = shape.getShape().liveShapes[0].geometry;
    near(g.x, g0.x + 15, 'shape geometry x'); near(g.y, g0.y + 25, 'shape geometry y');
    check(shape.bounds.x === b0.x + 15 && shape.bounds.y === b0.y + 25, 'shape bounds');
  )JS")
                  .arg(QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))));
  const auto* text = layer_with_id(window, ids["text"]);
  CHECK(text->bounds().x == text_bounds_before.x + 15 && text->bounds().y == text_bounds_before.y + 25);
  const auto text_after = text_transform(*text);
  CHECK(text_after.has_value());
  if (text_after.has_value()) {
    CHECK(std::abs((*text_after)[4] - ((*text_before)[4] + 15.0)) < 1e-9);
    CHECK(std::abs((*text_after)[5] - ((*text_before)[5] + 25.0)) < 1e-9);
  }
  const auto* pixels = layer_with_id(window, ids["pixels"]);
  CHECK(rect_equals(pixels->bounds(), 115, 125, 30, 30));
  CHECK(pixels->mask().has_value() && rect_equals(pixels->mask()->bounds, 115, 125, 30, 30));
}

// rerenderSmartObject draws an embedded smart object again from the file it stores. The
// layer's pixels are wiped first, standing in for a raster a PSD carried: the call brings the
// contents back where the layer was, and refuses a linked smart object and a plain layer.
void ui_script_rerender_smart_object_from_embedded_file() {
  MainWindow window;
  show_window(window);
  window.set_cli_automation_mode(true);
  const auto png = artifact_path(QStringLiteral("rerender-art.png"));
  {
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(QColor(20, 200, 40, 255));
    image.setDotsPerMeterX(2835);
    image.setDotsPerMeterY(2835);
    CHECK(image.save(png));
  }
  const auto ids = run(window, QStringLiteral(R"JS(
    var png = %1;
    var d = app.newDocument(200, 120);
    var embedded = d.addSmartObject(png, {x: 30, y: 40, name: 'Embedded'});
    var linked = d.addSmartObject(png, {linked: true, x: 100, y: 10, name: 'Linked'});
    patchy.setResult({embedded: embedded.id, linked: linked.id});
  )JS")
                                   .arg(literal(png)))
                       .toObject();
  auto* embedded = const_cast<patchy::Layer*>(layer_with_id(window, ids["embedded"]));
  CHECK(embedded != nullptr);
  if (embedded == nullptr) {
    return;
  }
  const auto bounds_before = std::as_const(*embedded).bounds();
  {
    auto& pixels = embedded->pixels();
    for (std::int32_t y = 0; y < pixels.height(); ++y) {
      auto row = pixels.row(y);
      std::fill(row.begin(), row.end(), std::uint8_t{0});
    }
  }
  run(window, QStringLiteral(R"JS(
    var ids = %1;
    var d = app.activeDocument;
    check(d.getLayer(ids.embedded).rerenderSmartObject() === 1, 'one layer re-rendered');
    var threw = false;
    try { d.getLayer(ids.linked).rerenderSmartObject(); } catch (e) { threw = true; }
    check(threw, 'a linked smart object is refused');
    threw = false;
    try { d.addLayer('plain').rerenderSmartObject(); } catch (e) { threw = true; }
    check(threw, 'a plain layer is refused');
  )JS")
                  .arg(QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))));
  const auto* after = layer_with_id(window, ids["embedded"]);
  CHECK(after != nullptr);
  if (after == nullptr) {
    return;
  }
  CHECK(rect_equals(after->bounds(), bounds_before.x, bounds_before.y, bounds_before.width, bounds_before.height));
  const auto& pixels = after->pixels();
  const auto* px = pixels.pixel(pixels.width() / 2, pixels.height() / 2);
  CHECK(px != nullptr && px[1] > 180 && px[0] < 60 && px[3] == 255);
}

// The placement quad follows an embedded and a linked smart object, Update Smart Object
// Content re-renders the linked one where the script put it, and the PSD reopens there.
void ui_script_move_carries_smart_object_quad() {
  MainWindow window;
  show_window(window);
  window.set_cli_automation_mode(true);
  const auto png = artifact_path(QStringLiteral("art.png"));
  const auto psd = artifact_path(QStringLiteral("smart-object.psd"));
  const auto write_png = [&png](QColor color) {
    QImage image(40, 20, QImage::Format_RGBA8888);
    image.fill(color);
    image.setDotsPerMeterX(2835);  // about 72 dpi, so the physical size is the pixel size
    image.setDotsPerMeterY(2835);
    CHECK(image.save(png));
  };
  write_png(QColor(20, 200, 40, 255));
  const auto ids = run(window, QStringLiteral(R"JS(
    var png = %1;
    var d = app.newDocument(400, 300);
    function movedQuad(layer, x, y, what) {
      var q0 = layer.getSmartObject().quad, b0 = layer.bounds;
      layer.moveTo(x, y);
      var q = layer.getSmartObject().quad;
      check(layer.bounds.x === x && layer.bounds.y === y, what + ' bounds ' + JSON.stringify(layer.bounds));
      for (var i = 0; i < 8; i++) {
        near(q[i], q0[i] + (i % 2 === 0 ? x - b0.x : y - b0.y), what + ' quad[' + i + ']');
      }
    }
    var embedded = d.addSmartObject(png, {x: 0, y: 0, width: 100, name: 'Embedded'});
    movedQuad(embedded, 200, 100, 'embedded');
    var linked = d.addSmartObject(png, {linked: true, x: 10, y: 10, name: 'Linked'});
    movedQuad(linked, 150, 220, 'linked');
    patchy.setResult({embedded: embedded.id, linked: linked.id});
  )JS")
                                   .arg(literal(png)))
                       .toObject();
  const auto embedded_bounds = layer_with_id(window, ids["embedded"])->bounds();
  const auto linked_bounds = layer_with_id(window, ids["linked"])->bounds();

  write_png(QColor(30, 40, 220, 255));
  run(window, QStringLiteral(R"JS(
    var psd = %1, ids = %2;
    var d = app.activeDocument;
    var linked = d.getLayer(ids.linked), quad = linked.getSmartObject().quad;
    check(linked.updateSmartObject() === 1, 'update count');
    check(linked.bounds.x === 150 && linked.bounds.y === 220, 'updated bounds ' + JSON.stringify(linked.bounds));
    check(JSON.stringify(linked.getSmartObject().quad) === JSON.stringify(quad), 'updated quad');
    var before = {Embedded: d.getLayer(ids.embedded).getSmartObject().quad, Linked: quad};
    check(d.saveAs(psd), 'save');
    d.close();
    var re = app.open(psd);
    ['Embedded', 'Linked'].forEach(function (name) {
      var quad = re.findLayer(name).getSmartObject().quad;
      for (var i = 0; i < 8; i++) { near(quad[i], before[name][i], name + ' reopened quad[' + i + ']'); }
    });
    patchy.setResult({embedded: re.findLayer('Embedded').id, linked: re.findLayer('Linked').id});
  )JS")
                  .arg(literal(psd), QString::fromUtf8(QJsonDocument(ids).toJson(QJsonDocument::Compact))));
  const auto reopened = window.script_engine_host().last_result().toObject();
  const auto reopened_embedded = layer_with_id(window, reopened["embedded"])->bounds();
  CHECK(rect_equals(reopened_embedded, embedded_bounds.x, embedded_bounds.y, embedded_bounds.width,
                    embedded_bounds.height));
  const auto* reopened_linked = layer_with_id(window, reopened["linked"]);
  CHECK(rect_equals(reopened_linked->bounds(), linked_bounds.x, linked_bounds.y, linked_bounds.width,
                    linked_bounds.height));
  // The update really re-rendered from the rewritten file.
  const auto& pixels = reopened_linked->pixels();
  const auto* px = pixels.pixel(pixels.width() / 2, pixels.height() / 2);
  CHECK(px != nullptr && px[2] > 200 && px[1] < 80);
}

}  // namespace

std::vector<patchy::test::TestCase> script_move_tests() {
  return {
      {"ui_script_move_carries_shape_geometry", ui_script_move_carries_shape_geometry},
      {"ui_script_move_carries_text_anchor", ui_script_move_carries_text_anchor},
      {"ui_script_move_follows_the_mask_link_rule", ui_script_move_follows_the_mask_link_rule},
      {"ui_script_move_group_carries_children", ui_script_move_group_carries_children},
      {"ui_script_move_carries_smart_object_quad", ui_script_move_carries_smart_object_quad},
      {"ui_script_rerender_smart_object_from_embedded_file", ui_script_rerender_smart_object_from_embedded_file},
  };
}
