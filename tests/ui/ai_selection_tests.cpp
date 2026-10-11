// Automatic selection in the app (docs/ai-selection.md): the Object Selection tool, Select >
// Subject and the script calls. Model results are CPU-dependent float math, so checks look
// at where the selection lands, never at exact pixels. Builds without the models skip.

#include "ui/canvas_widget.hpp"
#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/script_engine.hpp"

#include "ai/segmentation_engine.hpp"
#include "test_harness.hpp"
#include "ui_test_support.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QPushButton>

#include <cmath>
#include <cstdint>
#include <cstdio>

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

}  // namespace

std::vector<patchy::test::TestCase> ai_selection_ui_tests() {
  return {
      {"ui_object_select_box_and_clicks_select_on_release", ui_object_select_box_and_clicks_select_on_release},
      {"ui_object_select_tool_and_options_are_listed", ui_object_select_tool_and_options_are_listed},
      {"ui_select_subject_menu_selects_the_subject", ui_select_subject_menu_selects_the_subject},
      {"ui_script_automatic_selection_selects_objects", ui_script_automatic_selection_selects_objects},
  };
}
