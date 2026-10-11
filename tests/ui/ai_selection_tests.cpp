// Automatic selection in the app (docs/ai-selection.md): the Object Selection tool, Select >
// Subject and the script calls. Model results are CPU-dependent float math, so checks look
// at where the selection lands, never at exact pixels. Builds without the models skip.

#include "ui/canvas_widget.hpp"
#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/script_engine.hpp"

#include "ai/segmentation_engine.hpp"
#include "core/pixel_depth.hpp"
#include "test_harness.hpp"
#include "ui_test_access.hpp"
#include "ui_test_support.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QMenu>
#include <QStringList>
#include <QPushButton>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace {

using namespace patchy::test::ui;

bool automatic_selection_or_skip(const char* test_name) {
  patchy::ui::configure_automatic_selection_models();
  if (patchy::ai::segmentation_available()) {
    return true;
  }
  std::printf("[SKIP] %s (build without automatic selection)\n", test_name);
  return false;
}

// A self-authored scene: a shaded red disc (the one object) on mottled green noise.
patchy::Document disc_document(int width, int height, QPoint center, int radius) {
  patchy::Document document(width, height, patchy::PixelFormat::rgba8());
  patchy::PixelBuffer pixels(width, height, patchy::PixelFormat::rgba8());
  std::uint64_t state = 11;
  const auto next = [&state] {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31U);
  };
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      auto* px = pixels.pixel(x, y);
      const int noise = static_cast<int>(next() % 25U);
      const double dx = x + 0.5 - center.x();
      const double dy = y + 0.5 - center.y();
      if (dx * dx + dy * dy <= static_cast<double>(radius) * radius) {
        const double shade = 0.65 + 0.35 * std::max(0.0, -(dx + dy) / (radius * 1.5));
        px[0] = static_cast<std::uint8_t>(200.0 * shade);
        px[1] = static_cast<std::uint8_t>(40.0 * shade);
        px[2] = static_cast<std::uint8_t>(35.0 * shade);
      } else {
        const int blotch = static_cast<int>(18.0 * std::sin(x * 0.05) * std::cos(y * 0.07));
        px[0] = static_cast<std::uint8_t>(std::clamp(95 + noise + blotch, 0, 255));
        px[1] = static_cast<std::uint8_t>(std::clamp(120 + noise + blotch, 0, 255));
        px[2] = static_cast<std::uint8_t>(std::clamp(90 + noise, 0, 255));
      }
      px[3] = 255;
    }
  }
  document.add_pixel_layer("Scene", std::move(pixels));
  return document;
}

void ui_object_select_box_and_clicks_select_on_release() {
  if (!automatic_selection_or_skip("ui_object_select_box_and_clicks_select_on_release")) {
    return;
  }
  const QPoint center(170, 130);
  const int radius = 60;
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(360, 260, center, radius), QStringLiteral("Object Selection"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  require_action(window, "toolObjectSelectionAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas->tool() == patchy::ui::CanvasTool::ObjectSelect);
  canvas->set_selection_feather_radius(0);
  auto* undo_action = require_action_by_text(window, QStringLiteral("Undo"));
  const auto widget_point = [canvas](QPoint document_point) {
    return canvas->widget_position_for_document_point(document_point);
  };

  // A box drag: nothing is selected until release (the patent boundary), then the disc.
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(QPoint(95, 55)), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseMove, widget_point(QPoint(180, 150)), Qt::NoButton, Qt::LeftButton);
  CHECK(!canvas->has_selection());
  send_mouse(*canvas, QEvent::MouseMove, widget_point(QPoint(245, 205)), Qt::NoButton, Qt::LeftButton);
  CHECK(!canvas->has_selection());
  save_widget_artifact("ui_object_select_box_drag", *canvas);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(QPoint(245, 205)), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->has_selection());
  auto region = canvas->selected_document_region();
  CHECK(region.contains(center));
  CHECK(region.contains(center + QPoint(radius - 6, 0)));
  CHECK(!region.contains(center + QPoint(radius + 8, 0)));
  CHECK(!region.contains(QPoint(10, 10)));
  CHECK(!region.contains(QPoint(100, 60)));
  CHECK(canvas->object_select_encodes_started() == 1);
  save_widget_artifact("ui_object_select_box_selection", *canvas);

  undo_action->trigger();
  QApplication::processEvents();
  CHECK(!canvas->has_selection());

  // A click reuses the embedding (no second encode) and selects the disc under it.
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(center), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(center), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->object_select_encodes_started() == 1);
  CHECK(canvas->has_selection());
  region = canvas->selected_document_region();
  CHECK(region.contains(center + QPoint(0, radius - 6)));
  CHECK(!region.contains(QPoint(330, 230)));

  // Alt+click subtracts by plain boolean: the disc leaves the selection.
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(center), Qt::LeftButton, Qt::LeftButton, Qt::AltModifier);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(center), Qt::LeftButton, Qt::NoButton, Qt::AltModifier);
  QApplication::processEvents();
  CHECK(!canvas->selected_document_region().contains(center));

  // A change notification alone keeps the embedding (the pixels hash the same)...
  canvas->document_changed();
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(center), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(center), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->object_select_encodes_started() == 1);
  // ...and an actual pixel edit makes the next press encode again.
  {
    auto& host = window.script_engine_host();
    patchy::ui::ScriptEngineHost::RunOptions edit_options;
    edit_options.name = QStringLiteral("edit-pixels");
    (void)host.run_source(QStringLiteral("app.activeDocument.activeLayer.fillRect(0, 0, 12, 12, '#ffffff');"),
                          std::move(edit_options));
    QElapsedTimer edit_timer;
    edit_timer.start();
    while (host.run_active() && edit_timer.elapsed() < 15000) {
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
    }
    QApplication::processEvents();
    CHECK(!host.last_run_had_error());
  }
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(center), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(center), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->object_select_encodes_started() == 2);
}

void ui_object_select_tool_and_options_are_listed() {
  if (!automatic_selection_or_skip("ui_object_select_tool_and_options_are_listed")) {
    return;
  }
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(64, 64, QPoint(32, 32), 12), QStringLiteral("Options"));
  QApplication::processEvents();
  require_action(window, "toolObjectSelectionAction")->trigger();
  QApplication::processEvents();
  CHECK(require_canvas(window)->tool() == patchy::ui::CanvasTool::ObjectSelect);
  auto* sample_all = window.findChild<QCheckBox*>(QStringLiteral("objectSelectSampleAllLayersCheck"));
  auto* subject_button = window.findChild<QPushButton*>(QStringLiteral("selectSubjectButton"));
  CHECK(sample_all != nullptr && sample_all->isVisibleTo(&window));
  CHECK(subject_button != nullptr && subject_button->isVisibleTo(&window));
  sample_all->setChecked(false);
  CHECK(!require_canvas(window)->object_select_sample_all_layers());
  sample_all->setChecked(true);
  auto* subject_action = require_action(window, "selectSubjectAction");
  CHECK(subject_action->isVisible());
}

void ui_select_subject_menu_selects_the_subject() {
  if (!automatic_selection_or_skip("ui_select_subject_menu_selects_the_subject")) {
    return;
  }
  const QPoint center(150, 120);
  const int radius = 55;
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, center, radius), QStringLiteral("Select Subject"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  auto* subject_action = require_action(window, "selectSubjectAction");
  subject_action->trigger();
  QApplication::processEvents();
  CHECK(canvas->has_selection());
  const auto region = canvas->selected_document_region();
  CHECK(region.contains(center));
  CHECK(!region.contains(QPoint(8, 8)));
  CHECK(!region.contains(QPoint(290, 230)));
  save_widget_artifact("ui_select_subject_selection", *canvas);
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(!canvas->has_selection());
}

void ui_script_automatic_selection_selects_objects() {
  if (!automatic_selection_or_skip("ui_script_automatic_selection_selects_objects")) {
    return;
  }
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, QPoint(150, 120), 55), QStringLiteral("Script"));
  QApplication::processEvents();
  auto& host = window.script_engine_host();
  patchy::ui::ScriptEngineHost::RunOptions options;
  options.name = QStringLiteral("automatic-selection");
  (void)host.run_source(QStringLiteral(R"JS(
    var d = app.activeDocument;
    if (!d.selection.automaticSelectionAvailable) throw Error('unavailable');
    d.selection.objectSelect({rect: {x: 80, y: 50, width: 140, height: 140}});
    var b = d.selection.bounds;
    if (!b || b.x > 100 || b.x + b.width < 200) throw Error('box ' + JSON.stringify(b));
    d.selection.deselect();
    // Hard masks so subtracting the same object empties the selection exactly (soft edges
    // would correctly leave a faint ring: subtract is a * (1 - b)).
    d.selection.objectSelect({point: {x: 150, y: 120}, antialias: false});
    if (!d.selection.exists) throw Error('point');
    d.selection.objectSelect({point: {x: 150, y: 120}, operation: 'subtract', antialias: false});
    if (d.selection.exists) throw Error('subtract ' + JSON.stringify(d.selection.bounds));
    d.selection.selectSubject();
    b = d.selection.bounds;
    if (!b || b.x > 105 || b.y > 75 || b.x + b.width < 195) throw Error('subject ' + JSON.stringify(b));
    var threw = false;
    try { d.selection.objectSelect({rect: {x: 0, y: 0, width: 5, height: 5}, point: {x: 1, y: 1}}); }
    catch (e) { threw = true; }
    if (!threw) throw Error('box plus point accepted');
    threw = false;
    try { d.selection.objectSelect({point: {x: 1, y: 1}, operation: 'intersect'}); } catch (e) { threw = true; }
    if (!threw) throw Error('intersect accepted');
  )JS"), std::move(options));
  QElapsedTimer timer;
  timer.start();
  while (host.run_active() && timer.elapsed() < 120000) {
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  }
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  CHECK(!host.run_active());
  if (host.last_run_had_error()) {
    for (const auto& line : host.message_backlog()) {
      std::printf("  %s\n", line.toUtf8().constData());
    }
  }
  CHECK(!host.last_run_had_error());
}

void ui_select_background_selects_everything_but_the_subject() {
  if (!automatic_selection_or_skip("ui_select_background_selects_everything_but_the_subject")) {
    return;
  }
  const QPoint center(150, 120);
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, center, 55), QStringLiteral("Select Background"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  require_action(window, "selectBackgroundAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas->has_selection());
  const auto region = canvas->selected_document_region();
  CHECK(!region.contains(center));
  CHECK(region.contains(QPoint(5, 5)));
  CHECK(region.contains(QPoint(294, 234)));
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(!canvas->has_selection());
}

void ui_remove_background_masks_the_layer_in_one_undo_step() {
  if (!automatic_selection_or_skip("ui_remove_background_masks_the_layer_in_one_undo_step")) {
    return;
  }
  const QPoint center(150, 120);
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, center, 55), QStringLiteral("Remove Background"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  require_action(window, "layerRemoveBackgroundAction")->trigger();
  QApplication::processEvents();
  CHECK(!canvas->has_selection());  // the selection is untouched
  const auto& layer = std::as_const(patchy::ui::MainWindowTestAccess::document(window)).layers().front();
  CHECK(layer.mask().has_value());
  const auto& mask = *layer.mask();
  const auto mask_value = [&mask](QPoint point) {
    if (!mask.bounds.contains(point.x(), point.y())) {
      return static_cast<float>(mask.default_color) / 255.0F;
    }
    return patchy::coverage_at(mask.pixels, point.x() - mask.bounds.x, point.y() - mask.bounds.y);
  };
  CHECK(mask_value(center) > 0.9F);
  CHECK(mask_value(QPoint(5, 5)) < 0.1F);
  CHECK(mask_value(QPoint(294, 234)) < 0.1F);
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(!std::as_const(patchy::ui::MainWindowTestAccess::document(window)).layers().front().mask().has_value());
}

void ui_selection_tool_context_menu_offers_subject_commands() {
  if (!automatic_selection_or_skip("ui_selection_tool_context_menu_offers_subject_commands")) {
    return;
  }
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, QPoint(150, 120), 55), QStringLiteral("Context menu"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  require_action(window, "toolObjectSelectionAction")->trigger();
  QApplication::processEvents();
  const auto point = canvas->widget_position_for_document_point(QPoint(20, 20));
  send_mouse(*canvas, QEvent::MouseButtonPress, point, Qt::RightButton, Qt::RightButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, point, Qt::RightButton, Qt::NoButton);
  QApplication::processEvents();
  QMenu* menu = nullptr;
  for (auto* candidate : canvas->findChildren<QMenu*>(QStringLiteral("canvasContextMenu"))) {
    if (candidate->isVisible()) {
      menu = candidate;
    }
  }
  CHECK(menu != nullptr);
  QStringList names;
  for (auto* action : menu->actions()) {
    names << action->objectName();
  }
  CHECK(names.contains(QStringLiteral("selectSubjectAction")));
  CHECK(names.contains(QStringLiteral("selectBackgroundAction")));
  CHECK(names.contains(QStringLiteral("layerRemoveBackgroundAction")));
  menu->close();
  QApplication::processEvents();
}

void ui_select_object_in_marquee_selection_finds_the_object() {
  if (!automatic_selection_or_skip("ui_select_object_in_marquee_selection_finds_the_object")) {
    return;
  }
  const QPoint center(150, 120);
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, center, 55), QStringLiteral("Select Object"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  require_action_by_text(window, QStringLiteral("Marquee"))->trigger();
  QApplication::processEvents();
  const auto widget_point = [canvas](QPoint document_point) {
    return canvas->widget_position_for_document_point(document_point);
  };
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(QPoint(70, 40)), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseMove, widget_point(QPoint(150, 120)), Qt::NoButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseMove, widget_point(QPoint(232, 205)), Qt::NoButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(QPoint(232, 205)), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->selected_document_region().contains(QPoint(75, 45)));  // the marquee's corner

  // The right-click menu inside the selection offers Select Object.
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(center), Qt::RightButton, Qt::RightButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(center), Qt::RightButton, Qt::NoButton);
  QApplication::processEvents();
  QAction* select_object = nullptr;
  for (auto* menu : canvas->findChildren<QMenu*>(QStringLiteral("canvasContextMenu"))) {
    if (menu->isVisible()) {
      for (auto* action : menu->actions()) {
        if (action->objectName() == QStringLiteral("selectObjectInSelectionAction")) {
          select_object = action;
        }
      }
      menu->close();
    }
  }
  QApplication::processEvents();
  CHECK(select_object != nullptr);
  select_object->trigger();
  QApplication::processEvents();
  const auto region = canvas->selected_document_region();
  CHECK(region.contains(center));
  CHECK(!region.contains(QPoint(75, 45)));  // the object replaced the rectangle
  require_action_by_text(window, QStringLiteral("Undo"))->trigger();
  QApplication::processEvents();
  CHECK(canvas->selected_document_region().contains(QPoint(75, 45)));
}

// Runs a short script to completion and reports whether it ended without an error.
bool run_test_script(patchy::ui::MainWindow& window, const QString& source) {
  auto& host = window.script_engine_host();
  patchy::ui::ScriptEngineHost::RunOptions options;
  options.name = QStringLiteral("automatic-selection-sampling");
  (void)host.run_source(source, std::move(options));
  QElapsedTimer timer;
  timer.start();
  while (host.run_active() && timer.elapsed() < 120000) {
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  }
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  if (host.last_run_had_error()) {
    for (const auto& line : host.message_backlog()) {
      std::printf("  %s\n", line.toUtf8().constData());
    }
  }
  return !host.run_active() && !host.last_run_had_error();
}

// Seth's case: a vector shape layer over a photo, Sample All Layers off, a loose box drag.
// The tool must analyze the shape layer alone, never a stale all-layers analysis.
void ui_object_select_samples_the_active_shape_layer_alone() {
  if (!automatic_selection_or_skip("ui_object_select_samples_the_active_shape_layer_alone")) {
    return;
  }
  const QPoint center(150, 120);
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, center, 55), QStringLiteral("Shape layer"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  require_action(window, "toolObjectSelectionAction")->trigger();
  QApplication::processEvents();
  canvas->set_selection_feather_radius(0);
  const auto widget_point = [canvas](QPoint document_point) {
    return canvas->widget_position_for_document_point(document_point);
  };
  // An all-layers analysis first, so a stale embedding exists.
  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(center), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(center), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->object_select_encodes_started() == 1);
  CHECK(canvas->selected_document_region().contains(center));

  CHECK(run_test_script(window, QStringLiteral(
      "app.activeDocument.addShape('Rectangle 1', {type: 'rectangle', x: 10, y: 10, width: 80, height: 60},"
      " {fill: '#ffcc33'});")));
  auto* sample_all = window.findChild<QCheckBox*>(QStringLiteral("objectSelectSampleAllLayersCheck"));
  CHECK(sample_all != nullptr);
  sample_all->setChecked(false);
  QApplication::processEvents();

  send_mouse(*canvas, QEvent::MouseButtonPress, widget_point(QPoint(2, 2)), Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseMove, widget_point(QPoint(70, 60)), Qt::NoButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseMove, widget_point(QPoint(135, 105)), Qt::NoButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, widget_point(QPoint(135, 105)), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(canvas->object_select_encodes_started() == 2);  // the shape layer was analyzed anew
  const auto region = canvas->selected_document_region();
  CHECK(region.contains(QPoint(50, 40)));      // inside the rectangle
  CHECK(!region.contains(QPoint(120, 95)));    // the photo inside the box stays out
  CHECK(!region.contains(center));             // and so does the earlier disc selection
  const auto bounds = region.boundingRect();
  CHECK(bounds.left() >= 6 && bounds.top() >= 6 && bounds.right() <= 94 && bounds.bottom() <= 74);
}

void ui_select_subject_follows_sample_all_layers() {
  if (!automatic_selection_or_skip("ui_select_subject_follows_sample_all_layers")) {
    return;
  }
  const QPoint center(150, 120);
  auto document = disc_document(300, 240, center, 55);
  patchy::PixelBuffer top(300, 240, patchy::PixelFormat::rgba8());
  top.clear(0);
  for (int y = 20; y < 90; ++y) {
    for (int x = 215; x < 285; ++x) {
      auto* px = top.pixel(x, y);
      px[0] = 40;
      px[1] = 170;
      px[2] = 60;
      px[3] = 255;
    }
  }
  document.add_pixel_layer("Square", std::move(top));
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(std::move(document), QStringLiteral("Sampling"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  canvas->set_object_select_sample_all_layers(false);
  require_action(window, "selectSubjectAction")->trigger();
  QApplication::processEvents();
  auto region = canvas->selected_document_region();
  CHECK(region.contains(QPoint(250, 55)));  // the active layer's square
  CHECK(!region.contains(center));          // the disc on the layer below is not sampled

  CHECK(run_test_script(window, QStringLiteral(R"JS(
    var d = app.activeDocument;
    d.selection.deselect();
    d.selection.selectSubject({sampleAllLayers: false});
    var b = d.selection.bounds;
    if (!b || b.x < 200 || b.y > 30 || b.x + b.width < 280) throw Error('layer-only subject ' + JSON.stringify(b));
  )JS")));

  canvas->set_object_select_sample_all_layers(true);
  require_action(window, "selectSubjectAction")->trigger();
  QApplication::processEvents();
  CHECK(canvas->selected_document_region().contains(center));
}

void ui_automatic_selection_refuses_an_empty_layer_without_sample_all() {
  if (!automatic_selection_or_skip("ui_automatic_selection_refuses_an_empty_layer_without_sample_all")) {
    return;
  }
  auto document = disc_document(200, 160, QPoint(100, 80), 40);
  patchy::PixelBuffer empty(200, 160, patchy::PixelFormat::rgba8());
  empty.clear(0);
  document.add_pixel_layer("Empty", std::move(empty));
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(std::move(document), QStringLiteral("Empty layer"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  canvas->set_object_select_sample_all_layers(false);
  require_action(window, "selectSubjectAction")->trigger();
  QApplication::processEvents();
  CHECK(!canvas->has_selection());
  require_action(window, "toolObjectSelectionAction")->trigger();
  QApplication::processEvents();
  const auto point = canvas->widget_position_for_document_point(QPoint(100, 80));
  send_mouse(*canvas, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
  send_mouse(*canvas, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(!canvas->has_selection());
  CHECK(canvas->object_select_encodes_started() == 0);
}

void ui_script_select_background_and_remove_background() {
  if (!automatic_selection_or_skip("ui_script_select_background_and_remove_background")) {
    return;
  }
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(disc_document(300, 240, QPoint(150, 120), 55), QStringLiteral("Script background"));
  QApplication::processEvents();
  auto& host = window.script_engine_host();
  patchy::ui::ScriptEngineHost::RunOptions options;
  options.name = QStringLiteral("subject-background");
  (void)host.run_source(QStringLiteral(R"JS(
    var d = app.activeDocument;
    d.selection.selectBackground();
    var b = d.selection.bounds;
    if (!b || b.x !== 0 || b.y !== 0 || b.width !== 300 || b.height !== 240) throw Error('background ' + JSON.stringify(b));
    var threw = false;
    try { d.selection.selectBackground({inverse: true}); } catch (e) { threw = true; }
    if (!threw) throw Error('unknown key accepted');
    d.selection.deselect();
    d.activeLayer.removeBackground();
    if (d.selection.exists) throw Error('selection changed');
  )JS"), std::move(options));
  QElapsedTimer timer;
  timer.start();
  while (host.run_active() && timer.elapsed() < 120000) {
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  }
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
  CHECK(!host.run_active());
  if (host.last_run_had_error()) {
    for (const auto& line : host.message_backlog()) {
      std::printf("  %s\n", line.toUtf8().constData());
    }
  }
  CHECK(!host.last_run_had_error());
  const auto& layer = std::as_const(patchy::ui::MainWindowTestAccess::document(window)).layers().front();
  CHECK(layer.mask().has_value());
}

}  // namespace

std::vector<patchy::test::TestCase> ai_selection_ui_tests() {
  return {
      {"ui_object_select_box_and_clicks_select_on_release", ui_object_select_box_and_clicks_select_on_release},
      {"ui_object_select_tool_and_options_are_listed", ui_object_select_tool_and_options_are_listed},
      {"ui_select_subject_menu_selects_the_subject", ui_select_subject_menu_selects_the_subject},
      {"ui_script_automatic_selection_selects_objects", ui_script_automatic_selection_selects_objects},
      {"ui_select_background_selects_everything_but_the_subject",
       ui_select_background_selects_everything_but_the_subject},
      {"ui_remove_background_masks_the_layer_in_one_undo_step", ui_remove_background_masks_the_layer_in_one_undo_step},
      {"ui_selection_tool_context_menu_offers_subject_commands",
       ui_selection_tool_context_menu_offers_subject_commands},
      {"ui_script_select_background_and_remove_background", ui_script_select_background_and_remove_background},
      {"ui_select_object_in_marquee_selection_finds_the_object",
       ui_select_object_in_marquee_selection_finds_the_object},
      {"ui_object_select_samples_the_active_shape_layer_alone",
       ui_object_select_samples_the_active_shape_layer_alone},
      {"ui_select_subject_follows_sample_all_layers", ui_select_subject_follows_sample_all_layers},
      {"ui_automatic_selection_refuses_an_empty_layer_without_sample_all",
       ui_automatic_selection_refuses_an_empty_layer_without_sample_all},
  };
}
