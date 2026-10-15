#include "ui/action_icons.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/recent_file_list.hpp"
#include "core/adjustment_layer.hpp"
#include "core/contour_presets.hpp"
#include "core/gradient_presets.hpp"
#include "core/layer_metadata.hpp"
#include "core/pattern_presets.hpp"
#include "core/smart_filter.hpp"
#include "core/smart_filter_effects.hpp"
#include "core/smart_object.hpp"
#include "core/text_warp.hpp"
#include "ui/smart_object_render.hpp"
#include "core/layer_tree.hpp"
#include "core/palette.hpp"
#include "core/palette_presets.hpp"
#include "ui/palette_panel.hpp"
#include "ui/pattern_library.hpp"
#include "ui/pattern_manager_dialog.hpp"
#include "ui/photo_pattern_presets.hpp"
#include "ui/style_browser.hpp"
#include "ui/style_library.hpp"
#include "ui/style_manager_dialog.hpp"
#include "psd/asl_io.hpp"
#include "psd/psd_binary.hpp"
#include "psd/psd_layer_effects.hpp"

#include "core/style_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/blend_if_range_editor.hpp"
#include "ui/color_panel.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/document_float_window.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/curves_editor.hpp"
#include "ui/curves_presets.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/filter_look_library.hpp"
#include "ui/font_picker.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/gradient_library.hpp"
#include "ui/gradient_manager_dialog.hpp"
#include "formats/acv_curves_io.hpp"
#include "formats/bmp_document_io.hpp"
#include "formats/aseprite_document_io.hpp"
#include "formats/ico_document_io.hpp"
#include "formats/tga_document_io.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/localization.hpp"
#include "ui/main_window.hpp"
#include "ui/print_dialog.hpp"
#include "ui/selection_outline.hpp"
#include "ui/sprite_sheet_dialog.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/start_panel.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/icon_theme.hpp"
#include "ui/theme_file.hpp"
#include "ui/theme_palette.hpp"
#include "ui/theme_qss.hpp"
#include "ui/app_data_migration.hpp"
#include "ui/app_settings.hpp"
#include "ui/build_info.hpp"

#include "ui/background_workers.hpp"
#include "ui/network_mounts.hpp"
#include "ui/update_checker.hpp"
#include "ui/update_installer.hpp"
#include "ui/connector_freshness.hpp"
#include "ui/ai_control_paths.hpp"
#include "ui/appimage_environment.hpp"
#include "ui/mcp_attachment.hpp"
#include "ui/qt_paths.hpp"
#include "unicode_path_names.hpp"
#include "ui/visual_filter_gallery_dialog.hpp"
#include "ui/zoomable_image_preview.hpp"
#include "ui/zoom_status_bar.hpp"
#include "ui/single_instance.hpp"

#include "filters/builtin_filters.hpp"
#include "psd/psd_document_io.hpp"
#include "psd/psd_filter_effects.hpp"
#include "render/compositor.hpp"
#include "synthetic_dng.hpp"
#include "test_fonts.hpp"
#include "test_harness.hpp"
#include "local_psd_fixtures.hpp"
#include "patchy_version.hpp"

#include <QAbstractItemModel>
#include <QAbstractSpinBox>
#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDataStream>
#include <QDockWidget>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QInputDevice>
#include <QInputDialog>
#include <QKeyEvent>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocalServer>
#include <QLocalSocket>
#include <QList>
#include <QListView>
#include <QLayout>
#include <QListWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QLocale>
#include <QSizeGrip>
#include <QMetaObject>
#include <QMouseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QIODevice>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPolygonF>
#include <QThread>
#include <QPaintEvent>
#include <QPixmap>
#include <QPointingDevice>
#include <QProgressDialog>
#include <QPushButton>
#include <QStackedWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QStringList>
#include <QDateTime>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTabletEvent>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextLayout>
#include <QTimer>
#include <QAbstractButton>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVariant>
#include <QWheelEvent>
#include <QWindow>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ui_test_access.hpp"
#include "ui_test_groups.hpp"
#include "ui_test_support.hpp"

namespace {

using namespace patchy::test::ui;

void ui_main_window_renders_color_controls() {
  patchy::ui::MainWindow window;
  show_window(window);

  auto* foreground = window.findChild<QPushButton*>(QStringLiteral("foregroundColorButton"));
  auto* background = window.findChild<QPushButton*>(QStringLiteral("backgroundColorButton"));
  CHECK(foreground != nullptr);
  CHECK(background != nullptr);
  CHECK(foreground->text() == QStringLiteral("FG"));
  CHECK(background->text() == QStringLiteral("BG"));
  CHECK(!foreground->text().contains('#'));
  CHECK(!background->text().contains('#'));
  CHECK(window.findChild<QDockWidget*>(QStringLiteral("swatchesDock")) == nullptr);
  CHECK(window.findChild<QToolButton*>(QStringLiteral("swatchesDockCollapseButton")) == nullptr);
  CHECK(window.findChildren<QPushButton*>(QStringLiteral("swatchButton")).isEmpty());
  CHECK(window.findChild<QDockWidget*>(QStringLiteral("paletteDock")) != nullptr);
  const QStringList expected_menus = {QStringLiteral("File"),   QStringLiteral("Edit"),   QStringLiteral("Image"),
                                      QStringLiteral("Layer"),  QStringLiteral("Type"),   QStringLiteral("Select"),
                                      QStringLiteral("Filter"), QStringLiteral("Plugins"), QStringLiteral("View"),
                                      QStringLiteral("Window"), QStringLiteral("Help")};
  QStringList actual_menus;
  for (auto* action : window.menuBar()->actions()) {
    actual_menus << action->text().remove('&');
  }
  CHECK(actual_menus == expected_menus);
  CHECK(window.menuBar()->height() >= 30);
  // Frameless + custom chrome (badge, window buttons) exist only where Patchy draws its
  // own frame (Windows); macOS/Linux use the native frame and must NOT have them.
  CHECK(window.windowFlags().testFlag(Qt::FramelessWindowHint) ==
        patchy::ui::MainWindow::use_custom_window_chrome());
  auto* app_badge = window.menuBar()->findChild<QLabel*>(QStringLiteral("patchyBadge"));
  auto* window_close = window.findChild<QToolButton*>(QStringLiteral("windowCloseButton"));
  if (patchy::ui::MainWindow::use_custom_window_chrome()) {
    CHECK(app_badge != nullptr);
    CHECK(app_badge->pixmap(Qt::ReturnByValue).isNull() == false);
    CHECK(window_close != nullptr);
    CHECK(window_close->mapTo(&window, QPoint(window_close->width(), 0)).x() >= window.width() - 1);
  } else {
    CHECK(app_badge == nullptr);
    CHECK(window_close == nullptr);
  }
  CHECK(window.findChild<QAction*>(QStringLiteral("workspaceHomeAction")) == nullptr);
  CHECK(window.findChild<QAction*>(QStringLiteral("helpHomepageAction")) == nullptr);
  auto* recent_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentMenu"));
  CHECK(recent_menu != nullptr);
  auto* filter_menu = window.findChild<QMenu*>(QStringLiteral("filterMenu"));
  CHECK(filter_menu != nullptr);
  QStringList filter_action_texts;
  const std::function<void(QMenu*)> collect_filter_actions = [&](QMenu* menu) {
    for (auto* action : menu->actions()) {
      if (action->isSeparator()) {
        continue;
      }
      if (auto* submenu = action->menu(); submenu != nullptr) {
        collect_filter_actions(submenu);
        continue;
      }
      auto text = action->text();
      text.remove('&');
      filter_action_texts << text;
    }
  };
  collect_filter_actions(filter_menu);
  CHECK(filter_action_texts.contains(QStringLiteral("Soft Glow")));
  CHECK(filter_action_texts.contains(QStringLiteral("Punchy Color")));
  CHECK(filter_action_texts.contains(QStringLiteral("Noir")));
  CHECK(filter_action_texts.contains(QStringLiteral("Cinematic Matte")));
  CHECK(filter_action_texts.contains(QStringLiteral("Vintage Fade")));
  CHECK(filter_action_texts.contains(QStringLiteral("Twirl")));
  CHECK(filter_action_texts.contains(QStringLiteral("Clouds")));
  CHECK(filter_action_texts.contains(QStringLiteral("Pixel Mosaic")));
  CHECK(filter_action_texts.contains(QStringLiteral("Unsharp Mask")));
  CHECK(filter_action_texts.contains(QStringLiteral("Motion Blur")));
  CHECK(filter_action_texts.contains(QStringLiteral("Color Halftone")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Brightness +24")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Contrast +25%")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Brightness")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Contrast")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Auto Tone")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Auto Contrast")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Auto Color")));
  CHECK(!filter_action_texts.contains(QStringLiteral("Desaturate")));
  for (auto* action : filter_menu->actions()) {
    CHECK(!action->isIconVisibleInMenu());
  }
  auto* adjustments_menu = window.findChild<QMenu*>(QStringLiteral("imageAdjustmentsMenu"));
  CHECK(adjustments_menu != nullptr);
  QStringList adjustment_action_texts;
  for (auto* action : adjustments_menu->actions()) {
    CHECK(!action->isIconVisibleInMenu());
    if (!action->isSeparator()) {
      auto text = action->text();
      text.remove('&');
      adjustment_action_texts << text;
    }
  }
  CHECK(adjustment_action_texts.contains(QStringLiteral("Brightness/Contrast...")));
  CHECK(adjustment_action_texts.contains(QStringLiteral("Auto Tone")));
  CHECK(adjustment_action_texts.contains(QStringLiteral("Auto Contrast")));
  CHECK(adjustment_action_texts.contains(QStringLiteral("Auto Color")));
  CHECK(adjustment_action_texts.contains(QStringLiteral("Auto All")));
  CHECK(!adjustment_action_texts.contains(QStringLiteral("Brightness...")));
  CHECK(!adjustment_action_texts.contains(QStringLiteral("Contrast...")));
  CHECK(window.findChild<QAction*>(QStringLiteral("imageAdjustBrightnessAction")) == nullptr);
  CHECK(window.findChild<QAction*>(QStringLiteral("imageAdjustContrastAction")) == nullptr);
  auto* new_adjustments_menu = window.findChild<QMenu*>(QStringLiteral("layerNewAdjustmentMenu"));
  CHECK(new_adjustments_menu != nullptr);
  CHECK(require_action(window, "layerNewLevelsAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewCurvesAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewHueSaturationAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewColorBalanceAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewInvertAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewPosterizeAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewThresholdAdjustmentAction") != nullptr);
  CHECK(require_action(window, "layerNewBrightnessContrastAdjustmentAction") != nullptr);
  CHECK(window.findChild<QToolButton*>(QStringLiteral("layerNewAdjustmentButton")) != nullptr);
  // The Layer menu stays short enough for a small browser viewport (wasm) by
  // grouping the new-layer, mask, and arrange sets into submenus; the direct
  // row bound keeps it from silently regrowing.
  auto* layer_menu = window.findChild<QMenu*>(QStringLiteral("layerMenu"));
  CHECK(layer_menu != nullptr);
  CHECK(layer_menu->actions().size() <= 23);
  auto* layer_new_menu = window.findChild<QMenu*>(QStringLiteral("layerNewMenu"));
  CHECK(layer_new_menu != nullptr);
  CHECK(layer_new_menu->actions().contains(require_action(window, "layerNewAction")));
  CHECK(layer_new_menu->actions().contains(require_action(window, "layerNewFolderAction")));
  CHECK(layer_new_menu->actions().contains(require_action(window, "layerViaCopyAction")));
  CHECK(layer_new_menu->actions().contains(require_action(window, "layerViaCutAction")));
  auto* layer_mask_menu = window.findChild<QMenu*>(QStringLiteral("layerMaskMenu"));
  CHECK(layer_mask_menu != nullptr);
  for (const char* mask_action_name :
       {"layerAddMaskAction", "layerEditMaskAction", "layerMaskOverlayAction", "layerViewMaskAction",
        "layerLinkMaskAction", "layerDisableMaskAction", "layerInvertMaskAction", "layerApplyMaskAction",
        "layerDeleteMaskAction"}) {
    CHECK(layer_mask_menu->actions().contains(require_action(window, mask_action_name)));
  }
  auto* layer_arrange_menu = window.findChild<QMenu*>(QStringLiteral("layerArrangeMenu"));
  CHECK(layer_arrange_menu != nullptr);
  CHECK(layer_arrange_menu->actions().contains(require_action_by_text(window, QStringLiteral("Move Layer Up"))));
  CHECK(layer_arrange_menu->actions().contains(
      require_action_by_text(window, QStringLiteral("Flip Layer Horizontal"))));
  auto* feather_spin = window.findChild<QSpinBox*>(QStringLiteral("selectionFeatherSpin"));
  CHECK(feather_spin != nullptr);
  // Photoshop's 0..1000 px selection Feather.
  CHECK(feather_spin->maximum() == 1000);
  auto* feather_canvas = require_canvas(window);
  feather_canvas->set_selection_feather_radius(1000);
  CHECK(feather_canvas->selection_feather_radius() == 1000);
  feather_canvas->set_selection_feather_radius(5000);
  CHECK(feather_canvas->selection_feather_radius() == 1000);
  feather_canvas->set_selection_feather_radius(0);
  for (auto* button : window.findChildren<QPushButton*>()) {
    CHECK(button->text() != QStringLiteral("Select and Mask..."));
  }
  auto* options_bar = window.findChild<QToolBar*>(QStringLiteral("Options"));
  CHECK(options_bar != nullptr);
  CHECK(options_bar->height() >= 36);
  auto* tool_palette = window.findChild<QToolBar*>(QStringLiteral("toolPalette"));
  CHECK(tool_palette != nullptr);
  CHECK(tool_palette->width() <= 45);
  auto* marquee_button = window.findChild<QToolButton*>(QStringLiteral("marqueeToolButton"));
  CHECK(marquee_button != nullptr);
  CHECK(marquee_button->menu() != nullptr);
  CHECK(marquee_button->menu()->actions().size() == 4);  // Marquee, Elliptical, separator, Cycle
  CHECK(marquee_button->defaultAction() == require_action_by_text(window, QStringLiteral("Marquee")));
  auto* shape_button = window.findChild<QToolButton*>(QStringLiteral("shapeToolButton"));
  CHECK(shape_button != nullptr);
  CHECK(shape_button->menu() != nullptr);
  CHECK(shape_button->menu()->actions().size() == 7);  // Line/Rect/Ellipse/Polygon/Custom Shape + separator + Cycle
  CHECK(shape_button->defaultAction() == require_action_by_text(window, QStringLiteral("Rect")));

  save_widget_artifact("ui_main_window", window);
}

struct ToolPaletteOverflowSetup {
  QToolBar* palette{nullptr};
  QPushButton* foreground{nullptr};
  QPushButton* background{nullptr};
  QToolButton* quick_mask{nullptr};
  QToolButton* extension{nullptr};
  int fits_height{0};
};

// Looks up the tool palette widgets, verifies everything fits at a tall
// height, then shrinks the window to a ~40px palette shortage so the tail of
// the bottom cluster overflows into the extension button.
ToolPaletteOverflowSetup shrink_window_until_tool_palette_overflows(patchy::ui::MainWindow& window) {
  ToolPaletteOverflowSetup setup;
  setup.palette = window.findChild<QToolBar*>(QStringLiteral("toolPalette"));
  CHECK(setup.palette != nullptr);
  setup.foreground = window.findChild<QPushButton*>(QStringLiteral("foregroundColorButton"));
  setup.background = window.findChild<QPushButton*>(QStringLiteral("backgroundColorButton"));
  setup.quick_mask = window.findChild<QToolButton*>(QStringLiteral("quickMaskButton"));
  setup.extension = setup.palette->findChild<QToolButton*>(QStringLiteral("qt_toolbar_ext_button"));
  CHECK(setup.foreground != nullptr);
  CHECK(setup.background != nullptr);
  CHECK(setup.quick_mask != nullptr);
  CHECK(setup.extension != nullptr);

  // The chrome above and below the palette (menu bar, options bar, status bar)
  // keeps its height across window resizes, so palette height tracks window
  // height 1:1 and the fitting window height can be derived from the hint.
  const int chrome_height = window.height() - setup.palette->height();
  const int fits_height = setup.palette->sizeHint().height() + chrome_height;
  setup.fits_height = fits_height;

  window.resize(window.width(), fits_height + 24);
  QApplication::processEvents();
  process_events_for(60);
  CHECK(setup.quick_mask->isVisible());
  CHECK(!setup.extension->isVisible());

  const int short_height = fits_height - 40;
  CHECK(short_height > 0);
  window.resize(window.width(), short_height);
  QApplication::processEvents();
  process_events_for(60);
  // A minimum-size clamp refusing the resize would make the overflow
  // assertions vacuous, so pin the height that was actually applied.
  CHECK(window.height() == short_height);
  CHECK(setup.extension->isVisible());
  return setup;
}

void ui_tool_palette_overflow_hides_quick_mask_before_swatches() {
  patchy::ui::MainWindow window;
  show_window(window);
  const auto setup = shrink_window_until_tool_palette_overflows(window);

  // Vertical overflow hides items tail-first, and the bottom cluster is
  // ordered so Quick Mask, then Swap/Default, give way before the swatches.
  CHECK(!setup.quick_mask->isVisible());
  CHECK(setup.foreground->isVisible());
  CHECK(setup.background->isVisible());
  CHECK(setup.palette->width() <= 45);
}

void ui_tool_palette_extension_button_expands_palette() {
  patchy::ui::MainWindow window;
  // The expanded geometry is otherwise applied through the ~200ms main-window
  // widget animator; disable it so one event-loop settle suffices.
  window.setAnimated(false);
  show_window(window);
  const auto setup = shrink_window_until_tool_palette_overflows(window);
  CHECK(!setup.quick_mask->isVisible());

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() > 45; }));
  CHECK(setup.foreground->isVisible());
  CHECK(setup.background->isVisible());
  CHECK(setup.quick_mask->isVisible());
  // The reservation controller widens the palette's slot so the expansion
  // never overlays the canvas.
  CHECK(process_events_until(
      [&] { return !window.centralWidget()->geometry().intersects(setup.palette->geometry()); }));
  save_widget_artifact("ui_tool_palette_overflow_expanded", window);

  // Stock Qt collapses the expansion half a second after the pointer leaves
  // the bar; the palette keeps it open until it is explicitly toggled closed.
  QEvent leave_event(QEvent::Leave);
  QApplication::sendEvent(setup.palette, &leave_event);
  process_events_for(700);
  CHECK(setup.palette->width() > 45);
  CHECK(setup.quick_mask->isVisible());

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() <= 45; }));
  CHECK(!setup.quick_mask->isVisible());
  CHECK(setup.foreground->isVisible());
}

void ui_tool_palette_expansion_sticky_after_tool_pick() {
  patchy::ui::MainWindow window;
  window.setAnimated(false);
  show_window(window);
  const auto setup = shrink_window_until_tool_palette_overflows(window);

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() > 45; }));
  CHECK(process_events_until(
      [&] { return !window.centralWidget()->geometry().intersects(setup.palette->geometry()); }));

  // Picking a palette item keeps the expansion open: an auto-collapse here ate
  // the first click of the Zoom button's double-click, and the reserved-width
  // layout means the open expansion costs the canvas nothing it was not
  // already paying.
  auto* eraser_button = qobject_cast<QAbstractButton*>(
      setup.palette->widgetForAction(require_action(window, "toolEraserAction")));
  CHECK(eraser_button != nullptr);
  eraser_button->click();
  process_events_for(60);
  CHECK(setup.palette->width() > 45);
  CHECK(setup.quick_mask->isVisible());
  CHECK(require_action(window, "toolEraserAction")->isChecked());
  CHECK(!window.centralWidget()->geometry().intersects(setup.palette->geometry()));

  // The extension button stays the explicit way to close the expansion, and
  // closing releases the reserved width.
  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() <= 45; }));
  CHECK(setup.palette->minimumWidth() == 43);
  CHECK(!setup.quick_mask->isVisible());
  CHECK(setup.foreground->isVisible());
}

// Regression: with collapse-on-pick, the first click of a double-click on the
// expanded Zoom button closed the expansion, the button moved out from under
// the pointer, and the second click fell through to whatever was underneath.
void ui_tool_palette_expanded_zoom_double_click_sets_actual_pixels() {
  patchy::ui::MainWindow window;
  window.setAnimated(false);
  show_window(window);
  auto* canvas = require_canvas(window);
  canvas->set_zoom_centered(2.0);
  CHECK(canvas->zoom() == 2.0);
  const auto setup = shrink_window_until_tool_palette_overflows(window);

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() > 45; }));

  auto* zoom_button = window.findChild<QToolButton*>(QStringLiteral("zoomToolButton"));
  CHECK(zoom_button != nullptr);
  CHECK(zoom_button->isVisible());
  const QPoint center(zoom_button->width() / 2, zoom_button->height() / 2);
  click_widget_like_a_user(*zoom_button, center);
  // Give any (formerly fatal) deferred collapse a chance to run between the
  // two clicks, the way the double-click interval does for a real user.
  process_events_for(60);
  CHECK(setup.palette->width() > 45);
  CHECK(zoom_button->isVisible());
  send_double_click(*zoom_button, center);
  CHECK(canvas->zoom() == 1.0);
  CHECK(setup.palette->width() > 45);
}

void ui_tool_palette_expansion_collapses_when_window_fits_again() {
  patchy::ui::MainWindow window;
  window.setAnimated(false);
  show_window(window);
  const auto setup = shrink_window_until_tool_palette_overflows(window);

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() > 45; }));

  // Growing the window until everything fits leaves the expansion nothing to
  // reveal; the reservation controller closes it and releases the minimum.
  window.resize(window.width(), setup.fits_height + 24);
  CHECK(process_events_until([&] { return setup.palette->width() <= 45; }));
  CHECK(process_events_until([&] { return !setup.extension->isVisible(); }));
  CHECK(setup.palette->minimumWidth() == 43);
  CHECK(setup.quick_mask->isVisible());
}

// GitHub issue 88: with the main-window animator on (the default outside these
// tests), collapsing the expansion on Qt 6.11 left the buttons where the wide
// bar had centered them. Only a build against that Qt fails here without the
// fix (the Linux remote suite runs on it); Qt 6.8.3 pins the animation
// hand-back instead.
void ui_tool_palette_animated_collapse_realigns_buttons() {
  patchy::ui::MainWindow window;
  CHECK(window.isAnimated());
  show_window(window);
  const auto setup = shrink_window_until_tool_palette_overflows(window);

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() > 45; }));
  CHECK(process_events_until(
      [&] { return !window.centralWidget()->geometry().intersects(setup.palette->geometry()); }));
  process_events_for(400);
  CHECK(window.isAnimated());

  setup.extension->click();
  CHECK(process_events_until([&] { return setup.palette->width() <= 45; }));
  process_events_for(400);
  CHECK(window.isAnimated());
  CHECK(setup.palette->width() <= 45);
  CHECK(!window.centralWidget()->geometry().intersects(setup.palette->geometry()));
  int visible_children = 0;
  for (auto* child : setup.palette->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
    if (!child->isVisible()) {
      continue;
    }
    ++visible_children;
    CHECK(child->geometry().left() >= 0);
    CHECK(child->geometry().right() < setup.palette->width());
  }
  CHECK(visible_children > 10);
}

// Dissolve's dither threshold is a pure function of the document coordinate,
// which is exactly what the dirty-rect patch machinery needs: rendering a
// sub-rect must produce the same pixels as the same region of a full render.
// A stateful or scanline-counted noise field would pass every core unit test
// and still crawl under the canvas as the user pans.
void ui_dissolve_clipped_render_matches_full_render() {
  patchy::Document document(160, 120, patchy::PixelFormat::rgba8());
  document.add_pixel_layer("Background",
                           solid_pixels(160, 120, patchy::PixelFormat::rgba8(), QColor(Qt::black)));
  auto& top = document.add_pixel_layer(
      "Dissolve", solid_pixels(160, 120, patchy::PixelFormat::rgba8(), QColor(Qt::white)));
  top.set_blend_mode(patchy::BlendMode::Dissolve);
  top.set_opacity(0.5F);

  const auto full = patchy::ui::qimage_from_document(document, true);
  const std::array<QRect, 4> patches{QRect(0, 0, 71, 53), QRect(71, 0, 89, 53), QRect(0, 53, 71, 67),
                                     QRect(71, 53, 89, 67)};
  int painted = 0;
  for (const auto& patch : patches) {
    const auto clipped = patchy::ui::qimage_from_document_rect(document, patch, true);
    CHECK(clipped.width() == patch.width());
    CHECK(clipped.height() == patch.height());
    for (int y = 0; y < patch.height(); ++y) {
      for (int x = 0; x < patch.width(); ++x) {
        const auto expected = full.pixelColor(patch.x() + x, patch.y() + y);
        CHECK(clipped.pixelColor(x, y) == expected);
        // All or nothing, never a blended grey.
        CHECK(expected.red() == 0 || expected.red() == 255);
        painted += expected.red() == 255 ? 1 : 0;
      }
    }
  }
  // Roughly half of 160x120 dithered on.
  CHECK(painted > 9000);
  CHECK(painted < 10500);
}

void ui_window_force_refresh_action_rebuilds_cache() {
  patchy::Document document(180, 130, patchy::PixelFormat::rgba8());
  document.add_pixel_layer("Background", solid_pixels(180, 130, patchy::PixelFormat::rgba8(), QColor(Qt::white)));
  patchy::Layer layer(document.allocate_layer_id(), "Blue Block",
                      solid_pixels(44, 28, patchy::PixelFormat::rgba8(), QColor(20, 90, 235)));
  layer.set_bounds(patchy::Rect{42, 38, 44, 28});
  const auto expected = patchy::ui::qimage_from_document(document, true);

  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(std::move(document), QStringLiteral("Force Refresh"));
  QApplication::processEvents();
  auto* canvas = require_canvas(window);
  auto* action = require_action(window, "windowForceRefreshAction");
  CHECK(action->isEnabled());
  CHECK(action->shortcuts().contains(QKeySequence(Qt::Key_F5)));

  const auto before = canvas->render_cache_diagnostics();
  action->trigger();
  QApplication::processEvents();
  const auto after = canvas->render_cache_diagnostics();
  CHECK(after.full_refreshes == before.full_refreshes + 1);
  CHECK(after.forced_refreshes == before.forced_refreshes + 1);

  CHECK(color_close(canvas_pixel(*canvas, QPoint(48, 44)), expected.pixelColor(48, 44), 0));
  CHECK(color_close(canvas_pixel(*canvas, QPoint(16, 16)), expected.pixelColor(16, 16), 0));
  CHECK(window.statusBar()->currentMessage() == QStringLiteral("Forced refresh"));
  save_widget_artifact("ui_window_force_refresh", window);
}

void ui_canvas_ignores_opaque_psd_flat_cache_for_first_paint_transparency() {
  patchy::Document document(40, 30, patchy::PixelFormat::rgb8());
  auto layer_pixels = solid_pixels(40, 30, patchy::PixelFormat::rgba8(), Qt::transparent);
  fill_pixel_rect(layer_pixels, QRect(16, 12, 12, 10), QColor(230, 20, 30, 255));
  document.add_pixel_layer("Transparent Layer", std::move(layer_pixels));

  auto flat_composite = solid_pixels(40, 30, patchy::PixelFormat::rgb8(), Qt::black);
  fill_pixel_rect(flat_composite, QRect(16, 12, 12, 10), QColor(230, 20, 30));
  document.metadata().psd_flat_composite = std::move(flat_composite);

  patchy::ui::CanvasWidget canvas;
  canvas.resize(140, 100);
  canvas.set_document(&document);
  canvas.show();
  QApplication::processEvents();

  CHECK(color_close(canvas_pixel(canvas, QPoint(2, 2)), QColor(188, 188, 188), 1));
  CHECK(color_close(canvas_pixel(canvas, QPoint(14, 2)), QColor(236, 236, 236), 1));
  CHECK(color_close(canvas_pixel(canvas, QPoint(18, 14)), QColor(230, 20, 30), 1));
  CHECK(canvas.render_cache_diagnostics().full_refreshes == 1);
}

void ui_top_menu_items_highlight_on_hover() {
  patchy::ui::MainWindow window;
  show_window(window);

  auto* file_menu = window.menuBar()->actions().front()->menu();
  CHECK(file_menu != nullptr);
  auto* open_action = require_action(window, "fileOpenAction");
  CHECK(file_menu->actions().contains(open_action));

  file_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();
  const auto open_rect = file_menu->actionGeometry(open_action);
  CHECK(open_rect.isValid());
  const QPoint sample_point(open_rect.left() + 8, open_rect.center().y());
  const auto idle_color = file_menu->grab().toImage().pixelColor(sample_point);

  send_mouse(*file_menu, QEvent::MouseMove, open_rect.center(), Qt::NoButton, Qt::NoButton);
  if (file_menu->activeAction() != open_action) {
    file_menu->setActiveAction(open_action);
    QApplication::processEvents();
  }

  const auto hover_color = file_menu->grab().toImage().pixelColor(sample_point);
  CHECK(color_close(idle_color, QColor(58, 58, 58), 6));
  CHECK(color_close(hover_color, QColor(78, 111, 149), 6));
  CHECK(!color_close(idle_color, hover_color, 10));
  file_menu->close();
}

void ui_save_as_dialog_lists_recent_files() {
  ensure_artifact_dir();
  const auto first_path =
      QFileInfo(QStringLiteral("test-artifacts/recent-save-target.psd")).absoluteFilePath();
  const auto second_path =
      QFileInfo(QStringLiteral("test-artifacts/recent-save-backup.png")).absoluteFilePath();
  {
    QFile first(first_path);
    CHECK(first.open(QIODevice::WriteOnly));
    CHECK(first.write("patchy psd placeholder") > 0);
    QFile second(second_path);
    CHECK(second.open(QIODevice::WriteOnly));
    CHECK(second.write("patchy png placeholder") > 0);
  }

  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), QStringList{first_path, second_path});
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  auto* recent_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentMenu"));
  CHECK(recent_menu != nullptr);
  // The filter row leads the menu; the scrolling file list follows it.
  CHECK(recent_menu->actions().size() >= 2);
  CHECK(recent_menu->actions()[0]->objectName() == QStringLiteral("fileOpenRecentFilterAction"));
  CHECK(recent_menu->actions()[1]->objectName() == QStringLiteral("fileOpenRecentListAction"));
  CHECK(recent_menu_paths(*recent_menu) == (QStringList{first_path, second_path}));
  auto* first_row = recent_menu_list_item(*recent_menu, first_path);
  CHECK(first_row != nullptr);
  CHECK(first_row->text() == QFileInfo(first_path).fileName());
  CHECK(first_row->toolTip() == QDir::toNativeSeparators(first_path));

  auto* save_as_action = require_action(window, "fileSaveAsAction");
  CHECK(save_as_action->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("saveAsFileDialog")));
    CHECK(dialog != nullptr);
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("saveAsRecentFileNameCombo"));
    CHECK(combo != nullptr);
    CHECK(combo->isEditable());
    CHECK(combo->count() == 2);
    CHECK(combo->itemText(0) == first_path);
    CHECK(combo->itemData(0).toString() == first_path);
    CHECK(combo->itemData(0, Qt::ToolTipRole).toString() == first_path);
    CHECK(combo->itemText(1) == second_path);
    CHECK(combo->itemData(1).toString() == second_path);
    combo->setCurrentIndex(1);
    QApplication::processEvents();
    const auto selected_files = dialog->selectedFiles();
    CHECK(!selected_files.isEmpty());
    CHECK(QFileInfo(selected_files.first()).absoluteFilePath() == second_path);
    saw_dialog = true;
    dialog->reject();
  });
  save_as_action->trigger();
  CHECK(saw_dialog);
}

void ui_open_recent_lists_two_hundred_files_in_one_scrolling_list() {
  ensure_artifact_dir();
  QStringList recent_files;
  for (int i = 0; i < 205; ++i) {
    const auto path =
        QFileInfo(QStringLiteral("test-artifacts/recent-file-%1.psd").arg(i, 3, 10, QLatin1Char('0')))
            .absoluteFilePath();
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write("patchy recent placeholder") > 0);
    recent_files << path;
  }

  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), recent_files);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  // One list of every entry (the persisted cap is 200), no paged submenus and no
  // per-file menu rows: the menu stays a compact box beside the File menu.
  auto* recent_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentMenu"));
  CHECK(recent_menu != nullptr);
  auto* list = recent_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentList"));
  CHECK(list != nullptr);
  const auto all_paths = recent_menu_paths(*recent_menu);
  CHECK(all_paths.size() == 200);
  for (int i = 0; i < all_paths.size(); ++i) {
    CHECK(all_paths[i] == recent_files[i]);
  }
  CHECK(!all_paths.contains(recent_files[200]));
  for (auto* action : recent_menu->actions()) {
    CHECK(action->menu() == nullptr);
    CHECK(action->data().toString().isEmpty());
  }
  CHECK(recent_menu->actions().contains(require_action(window, "fileClearRecentAction")));

  recent_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();
  CHECK(list->isVisible());
  CHECK(list->verticalScrollBar()->maximum() > 0);  // the rest scroll
  CHECK(list->height() <= 12 * 40 + 14);
  CHECK(recent_menu->width() <= 640);
  if (auto* screen = QApplication::primaryScreen(); screen != nullptr) {
    CHECK(recent_menu->height() <= screen->availableGeometry().height());
  }
  save_widget_artifact("ui_open_recent_scrolling_list", *recent_menu);

  // Right-click on a row offers the shared recent-entry context menu.
  QApplication::clipboard()->clear();
  auto* older_row = recent_menu_list_item(*recent_menu, recent_files[50]);
  CHECK(older_row != nullptr);
  list->scrollToItem(older_row);
  QApplication::processEvents();
  bool saw_context_menu = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* menu = qobject_cast<QMenu*>(widget);
      if (menu == nullptr || menu->objectName() != QStringLiteral("recentFileContextMenu")) {
        continue;
      }
      auto* copy_action = find_menu_action_by_text(*menu, QStringLiteral("Copy File Path"));
      CHECK(copy_action != nullptr);
      CHECK(copy_action->objectName() == QStringLiteral("recentFileCopyPathAction"));
      copy_action->trigger();
      menu->close();
      saw_context_menu = true;
      return;
    }
    CHECK(false);
  });
  const auto context_point = list->visualItemRect(older_row).center();
  QContextMenuEvent context_event(QContextMenuEvent::Mouse, context_point,
                                  list->viewport()->mapToGlobal(context_point));
  QApplication::sendEvent(list->viewport(), &context_event);
  QApplication::processEvents();
  CHECK(saw_context_menu);
  CHECK(QApplication::clipboard()->text() == QDir::toNativeSeparators(recent_files[50]));

  // A click opens the row; a vanished file reports it and leaves the list.
  recent_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();
  list = recent_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentList"));
  CHECK(list != nullptr);
  older_row = recent_menu_list_item(*recent_menu, recent_files[50]);
  CHECK(older_row != nullptr);
  list->scrollToItem(older_row);
  QApplication::processEvents();
  CHECK(QFile::remove(recent_files[50]));
  QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(older_row).center());
  QApplication::processEvents();  // the deferred open fires
  QApplication::processEvents();
  CHECK(!recent_menu->isVisible());
  CHECK(window.statusBar()->currentMessage() == QStringLiteral("Recent file is missing"));
  const auto refreshed_paths = recent_menu_paths(*recent_menu);
  CHECK(refreshed_paths.size() == 199);
  CHECK(!refreshed_paths.contains(recent_files[50]));
}

void ui_open_recent_filter_narrows_entries_and_opens_first_match() {
  ensure_artifact_dir();
  QStringList recent_files;
  for (int i = 0; i < 120; ++i) {
    const bool beta = (i % 2) == 1;
    const auto pattern = beta ? QStringLiteral("test-artifacts/recent-filter-beta-%1.png")
                              : QStringLiteral("test-artifacts/recent-filter-alpha-%1.psd");
    const auto path = QFileInfo(pattern.arg(i, 3, 10, QLatin1Char('0'))).absoluteFilePath();
    if (i == 1 || i == 3) {
      // Enter opens these for real, so they must be loadable images.
      QImage image(48, 32 + i, QImage::Format_RGB32);
      image.fill(QColor(90, 150, 210));
      CHECK(image.save(path));
    } else {
      QFile file(path);
      CHECK(file.open(QIODevice::WriteOnly));
      CHECK(file.write("patchy recent placeholder") > 0);
    }
    recent_files << path;
  }

  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), recent_files);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window_empty(window);

  auto* recent_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentMenu"));
  CHECK(recent_menu != nullptr);
  CHECK(!recent_menu->actions().isEmpty());
  auto* filter_action = recent_menu->actions().front();
  CHECK(filter_action->objectName() == QStringLiteral("fileOpenRecentFilterAction"));
  auto* filter_edit = recent_menu->findChild<QLineEdit*>(QStringLiteral("fileOpenRecentFilterEdit"));
  CHECK(filter_edit != nullptr);
  auto* list = recent_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentList"));
  CHECK(list != nullptr);

  recent_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();
  CHECK(filter_edit->text().isEmpty());
  const auto list_height = list->height();
  const auto menu_size = recent_menu->size();

  // Baseline: every entry, newest first and highlighted.
  CHECK(recent_menu_paths(*recent_menu).size() == 120);
  CHECK(list->currentRow() == 0);

  // Typing narrows to matches from the whole list without resizing the menu.
  filter_edit->setText(QStringLiteral("beta"));
  QApplication::processEvents();
  const auto filtered = recent_menu_paths(*recent_menu);
  CHECK(filtered.size() == 60);
  for (const auto& path : filtered) {
    CHECK(path.contains(QStringLiteral("beta")));
  }
  CHECK(filtered.front() == recent_files[1]);
  CHECK(filtered.contains(recent_files[119]));
  CHECK(list->currentRow() == 0);
  CHECK(list->height() == list_height);
  CHECK(recent_menu->size() == menu_size);

  // Space-separated words AND together, matching case-insensitively.
  filter_edit->setText(QStringLiteral("BETA 003"));
  QApplication::processEvents();
  CHECK(recent_menu_paths(*recent_menu) == QStringList{recent_files[3]});

  // Arrow keys move the list highlight while the edit keeps focus.
  filter_edit->setText(QStringLiteral("beta"));
  QApplication::processEvents();
  send_key(*filter_edit, Qt::Key_Down);
  CHECK(list->currentRow() == 1);
  CHECK(list->currentItem()->data(patchy::ui::kRecentPathRole).toString() == recent_files[3]);
  send_key(*filter_edit, Qt::Key_Up);
  CHECK(list->currentRow() == 0);

  // No matches: only the inert placeholder row shows.
  filter_edit->setText(QStringLiteral("zzz-no-such-file"));
  QApplication::processEvents();
  CHECK(recent_menu_paths(*recent_menu).isEmpty());
  CHECK(list->count() == 1);
  CHECK(list->item(0)->text() == QStringLiteral("No matching recent files"));
  CHECK(list->item(0)->flags() == Qt::NoItemFlags);
  CHECK(list->height() == list_height);

  // Right-click on the filter row or the placeholder must not open the context menu.
  const auto filter_point = recent_menu->actionGeometry(filter_action).center();
  QContextMenuEvent filter_context_event(QContextMenuEvent::Mouse, filter_point,
                                         recent_menu->mapToGlobal(filter_point));
  QApplication::sendEvent(recent_menu, &filter_context_event);
  const auto placeholder_point = list->visualItemRect(list->item(0)).center();
  QContextMenuEvent placeholder_context_event(QContextMenuEvent::Mouse, placeholder_point,
                                              list->viewport()->mapToGlobal(placeholder_point));
  QApplication::sendEvent(list->viewport(), &placeholder_context_event);
  QApplication::processEvents();
  CHECK(!top_level_widget_exists(QStringLiteral("recentFileContextMenu")));

  // Clearing the filter restores every entry.
  filter_edit->clear();
  QApplication::processEvents();
  CHECK(recent_menu_paths(*recent_menu).size() == 120);
  CHECK(recent_menu_paths(*recent_menu).front() == recent_files[0]);

  // Enter opens the highlighted row: the second beta match, a real 48 x 35 PNG.
  auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget());
  CHECK(tabs != nullptr);
  CHECK(tabs->count() == 0);
  filter_edit->setText(QStringLiteral("beta"));
  QApplication::processEvents();
  send_key(*filter_edit, Qt::Key_Down);
  send_key(*filter_edit, Qt::Key_Return);
  QApplication::processEvents();  // the deferred open fires
  QApplication::processEvents();
  CHECK(!recent_menu->isVisible());
  CHECK(tabs->count() == 1);
  auto* info = window.findChild<QLabel*>(QStringLiteral("documentInfoLabel"));
  CHECK(info != nullptr);
  CHECK(info->text().contains(QStringLiteral("48 x 35 px")));
}

// A recent-folder pick opens the Open dialog on a later event-loop turn (after the menu
// closes), so a one-shot timer queued beside it can fire first and leave the modal
// waiting forever. This polls until the dialog is actually showing, then closes it, and
// returns the directory it opened in (empty when it never appeared).
QString answer_deferred_open_dialog(const std::function<void()>& trigger) {
  QString opened_in;
  bool answered = false;
  QTimer poll;
  poll.setInterval(10);
  QObject::connect(&poll, &QTimer::timeout, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("openFileDialog")));
    if (dialog == nullptr || !dialog->isVisible()) {
      return;
    }
    poll.stop();
    opened_in = QFileInfo(dialog->directory().absolutePath()).absoluteFilePath();
    answered = true;
    dialog->reject();
  });
  poll.start();
  trigger();
  process_events_until([&] { return answered; }, 10000);
  poll.stop();
  return opened_in;
}

void ui_open_recent_folders_list_two_hundred_folders_with_filter() {
  ensure_artifact_dir();
  QStringList recent_folders;
  for (int i = 0; i < 205; ++i) {
    const auto folder =
        QFileInfo(QStringLiteral("test-artifacts/recent-folder-%1").arg(i, 3, 10, QLatin1Char('0')))
            .absoluteFilePath();
    CHECK(QDir().mkpath(folder));
    recent_folders << folder;
  }

  SettingsValueRestorer recent_folders_restorer(QStringLiteral("recentFolders"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFolders"), recent_folders);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  // Open Recent Folder matches Open Recent File: a filter row over one scrolling list
  // of every entry (the persisted cap is 200), no paged submenus or per-folder rows.
  auto* folders_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentFolderMenu"));
  CHECK(folders_menu != nullptr);
  CHECK(folders_menu->actions().size() >= 2);
  CHECK(folders_menu->actions()[0]->objectName() == QStringLiteral("fileOpenRecentFolderFilterAction"));
  CHECK(folders_menu->actions()[1]->objectName() == QStringLiteral("fileOpenRecentFolderListAction"));
  auto* filter_edit = folders_menu->findChild<QLineEdit*>(QStringLiteral("fileOpenRecentFolderFilterEdit"));
  CHECK(filter_edit != nullptr);
  CHECK(filter_edit->placeholderText() == QStringLiteral("Filter recent folders..."));
  auto* list = folders_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentFolderList"));
  CHECK(list != nullptr);
  const auto all_paths = recent_menu_paths(*folders_menu);
  CHECK(all_paths.size() == 200);
  for (int i = 0; i < all_paths.size(); ++i) {
    CHECK(all_paths[i] == recent_folders[i]);
  }
  CHECK(!all_paths.contains(recent_folders[200]));
  for (auto* action : folders_menu->actions()) {
    CHECK(action->menu() == nullptr);
    CHECK(action->data().toString().isEmpty());
  }
  CHECK(folders_menu->actions().contains(require_action(window, "fileClearRecentFoldersAction")));
  // Two-line rows: the folder's name over its parent directory.
  auto* first_row = recent_menu_list_item(*folders_menu, recent_folders[0]);
  CHECK(first_row != nullptr);
  CHECK(first_row->text() == QStringLiteral("recent-folder-000"));
  CHECK(first_row->toolTip() == QDir::toNativeSeparators(recent_folders[0]));

  folders_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();
  CHECK(list->isVisible());
  CHECK(list->verticalScrollBar()->maximum() > 0);
  CHECK(list->height() <= 12 * 40 + 14);
  CHECK(folders_menu->width() <= 640);
  if (auto* screen = QApplication::primaryScreen(); screen != nullptr) {
    CHECK(folders_menu->height() <= screen->availableGeometry().height());
  }
  save_widget_artifact("ui_open_recent_folder_scrolling_list", *folders_menu);
  const auto list_height = list->height();

  // The filter narrows the whole list without resizing it; arrows move the highlight.
  filter_edit->setText(QStringLiteral("folder-05"));
  QApplication::processEvents();
  const auto filtered = recent_menu_paths(*folders_menu);
  CHECK(filtered.size() == 10);
  CHECK(filtered.front() == recent_folders[50]);
  CHECK(filtered.back() == recent_folders[59]);
  CHECK(list->height() == list_height);
  CHECK(list->currentRow() == 0);
  send_key(*filter_edit, Qt::Key_Down);
  CHECK(list->currentItem()->data(patchy::ui::kRecentPathRole).toString() == recent_folders[51]);

  filter_edit->setText(QStringLiteral("zzz-no-such-folder"));
  QApplication::processEvents();
  CHECK(recent_menu_paths(*folders_menu).isEmpty());
  CHECK(list->count() == 1);
  CHECK(list->item(0)->text() == QStringLiteral("No matching recent folders"));

  // Right-click on a row offers Copy Folder Path.
  filter_edit->setText(QStringLiteral("folder-05"));
  QApplication::processEvents();
  QApplication::clipboard()->clear();
  auto* row = recent_menu_list_item(*folders_menu, recent_folders[50]);
  CHECK(row != nullptr);
  bool saw_context_menu = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* menu = qobject_cast<QMenu*>(widget);
      if (menu == nullptr || menu->objectName() != QStringLiteral("recentFileContextMenu")) {
        continue;
      }
      auto* copy_action = find_menu_action_by_text(*menu, QStringLiteral("Copy Folder Path"));
      CHECK(copy_action != nullptr);
      CHECK(copy_action->objectName() == QStringLiteral("recentFolderCopyPathAction"));
      copy_action->trigger();
      menu->close();
      saw_context_menu = true;
      return;
    }
    CHECK(false);
  });
  const auto context_point = list->visualItemRect(row).center();
  QContextMenuEvent context_event(QContextMenuEvent::Mouse, context_point,
                                  list->viewport()->mapToGlobal(context_point));
  QApplication::sendEvent(list->viewport(), &context_event);
  QApplication::processEvents();
  CHECK(saw_context_menu);
  CHECK(QApplication::clipboard()->text() == QDir::toNativeSeparators(recent_folders[50]));

  // Enter opens the Open dialog in the highlighted folder, after the menu closes.
  folders_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();
  filter_edit = folders_menu->findChild<QLineEdit*>(QStringLiteral("fileOpenRecentFolderFilterEdit"));
  CHECK(filter_edit != nullptr);
  filter_edit->setText(QStringLiteral("folder-05"));
  QApplication::processEvents();
  send_key(*filter_edit, Qt::Key_Down);
  const auto opened_in = answer_deferred_open_dialog([&] {
    send_key(*filter_edit, Qt::Key_Return);
    CHECK(!folders_menu->isVisible());
  });
  CHECK(opened_in == QFileInfo(recent_folders[51]).absoluteFilePath());
}

void ui_recent_file_context_menu_copies_path() {
  ensure_artifact_dir();
  const auto first_path = QFileInfo(QStringLiteral("test-artifacts/recent-copy-target.psd")).absoluteFilePath();
  {
    QFile first(first_path);
    CHECK(first.open(QIODevice::WriteOnly));
    CHECK(first.write("patchy psd placeholder") > 0);
  }

  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), QStringList{first_path});
    settings.sync();
  }

  QApplication::clipboard()->clear();

  patchy::ui::MainWindow window;
  show_window(window);

  auto* recent_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentMenu"));
  CHECK(recent_menu != nullptr);
  CHECK(recent_menu->contextMenuPolicy() == Qt::CustomContextMenu);
  CHECK(!recent_menu->actions().isEmpty());
  // The file entry is a row of the menu's scrolling list.
  auto* recent_list = recent_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentList"));
  CHECK(recent_list != nullptr);
  auto* recent_row = recent_menu_list_item(*recent_menu, first_path);
  CHECK(recent_row != nullptr);

  recent_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();

  bool saw_context_menu = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* menu = qobject_cast<QMenu*>(widget);
      if (menu == nullptr || menu->objectName() != QStringLiteral("recentFileContextMenu")) {
        continue;
      }
      auto* copy_action = find_menu_action_by_text(*menu, QStringLiteral("Copy File Path"));
      CHECK(copy_action != nullptr);
      CHECK(copy_action->objectName() == QStringLiteral("recentFileCopyPathAction"));
      auto* explorer_action = find_menu_action_by_text(*menu, QStringLiteral("Open in File Explorer"));
      CHECK(explorer_action != nullptr);
      CHECK(explorer_action->objectName() == QStringLiteral("recentFileOpenInExplorerAction"));
      copy_action->trigger();
      menu->close();
      saw_context_menu = true;
      return;
    }
    CHECK(false);
  });

  const auto context_point = recent_list->visualItemRect(recent_row).center();
  QContextMenuEvent context_event(QContextMenuEvent::Mouse, context_point,
                                  recent_list->viewport()->mapToGlobal(context_point));
  QApplication::sendEvent(recent_list->viewport(), &context_event);
  QApplication::processEvents();

  CHECK(saw_context_menu);
  CHECK(QApplication::clipboard()->text() == QDir::toNativeSeparators(first_path));
  recent_menu->close();
}

void ui_recent_folder_context_menu_copies_path_and_offers_explorer() {
  ensure_artifact_dir();
  const auto folder = QFileInfo(QStringLiteral("test-artifacts/recent-folder-context")).absoluteFilePath();
  CHECK(QDir().mkpath(folder));

  SettingsValueRestorer recent_folders_restorer(QStringLiteral("recentFolders"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFolders"), QStringList{folder});
    settings.sync();
  }

  QApplication::clipboard()->clear();

  patchy::ui::MainWindow window;
  show_window(window);

  auto* folders_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentFolderMenu"));
  CHECK(folders_menu != nullptr);
  CHECK(folders_menu->contextMenuPolicy() == Qt::CustomContextMenu);
  CHECK(!folders_menu->actions().isEmpty());
  auto* folder_list = folders_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentFolderList"));
  CHECK(folder_list != nullptr);
  auto* folder_row = recent_menu_list_item(*folders_menu, folder);
  CHECK(folder_row != nullptr);

  folders_menu->popup(window.mapToGlobal(QPoint(40, 40)));
  QApplication::processEvents();

  bool saw_context_menu = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* menu = qobject_cast<QMenu*>(widget);
      if (menu == nullptr || menu->objectName() != QStringLiteral("recentFileContextMenu")) {
        continue;
      }
      auto* copy_action = find_menu_action_by_text(*menu, QStringLiteral("Copy Folder Path"));
      CHECK(copy_action != nullptr);
      CHECK(copy_action->objectName() == QStringLiteral("recentFolderCopyPathAction"));
      // Confirm the Explorer option is present, but do not trigger it (it would spawn explorer.exe).
      auto* explorer_action = find_menu_action_by_text(*menu, QStringLiteral("Open in File Explorer"));
      CHECK(explorer_action != nullptr);
      CHECK(explorer_action->objectName() == QStringLiteral("recentFolderOpenInExplorerAction"));
      copy_action->trigger();
      menu->close();
      saw_context_menu = true;
      return;
    }
    CHECK(false);
  });

  const auto context_point = folder_list->visualItemRect(folder_row).center();
  QContextMenuEvent context_event(QContextMenuEvent::Mouse, context_point,
                                  folder_list->viewport()->mapToGlobal(context_point));
  QApplication::sendEvent(folder_list->viewport(), &context_event);
  QApplication::processEvents();

  CHECK(saw_context_menu);
  CHECK(QApplication::clipboard()->text() == QDir::toNativeSeparators(folder));
  folders_menu->close();
}

void ui_save_as_remembers_last_save_directory_between_windows() {
  ensure_artifact_dir();
  const auto remembered_dir = QFileInfo(QStringLiteral("test-artifacts/remembered-save-dir")).absoluteFilePath();
  CHECK(QDir().mkpath(remembered_dir));
  const auto saved_path = QDir(remembered_dir).filePath(QStringLiteral("remembered-save.psd"));
  QFile::remove(saved_path);

  SettingsValueRestorer last_save_directory_restorer(QStringLiteral("lastSaveDirectory"));
  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));

  {
    patchy::ui::MainWindow window;
    show_window(window);
    bool saved = false;
    QTimer::singleShot(0, [&] {
      auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("saveAsFileDialog")));
      CHECK(dialog != nullptr);
      dialog->setDirectory(remembered_dir);
      dialog->selectFile(saved_path);
      saved = true;
      static_cast<QDialog*>(dialog)->accept();
    });
    require_action(window, "fileSaveAsAction")->trigger();
    CHECK(saved);
    CHECK(QFileInfo::exists(saved_path));
  }

  {
    auto settings = patchy::ui::app_settings();
    CHECK(QFileInfo(settings.value(QStringLiteral("lastSaveDirectory")).toString()).absoluteFilePath() ==
          QFileInfo(remembered_dir).absoluteFilePath());
  }

  patchy::ui::MainWindow next_window;
  show_window(next_window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("saveAsFileDialog")));
    CHECK(dialog != nullptr);
    CHECK(QFileInfo(dialog->directory().absolutePath()).absoluteFilePath() ==
          QFileInfo(remembered_dir).absoluteFilePath());
    const auto selected_files = dialog->selectedFiles();
    CHECK(!selected_files.isEmpty());
    CHECK(QFileInfo(selected_files.first()).absolutePath() == QFileInfo(remembered_dir).absoluteFilePath());
    saw_dialog = true;
    dialog->reject();
  });
  require_action(next_window, "fileSaveAsAction")->trigger();
  CHECK(saw_dialog);
}

void ui_open_remembers_last_directory_and_lists_recent_folders() {
  ensure_artifact_dir();
  const auto folder_a = QFileInfo(QStringLiteral("test-artifacts/recent-open-dir-a")).absoluteFilePath();
  const auto folder_b = QFileInfo(QStringLiteral("test-artifacts/recent-open-dir-b")).absoluteFilePath();
  const auto missing_folder = QFileInfo(QStringLiteral("test-artifacts/recent-open-dir-missing")).absoluteFilePath();
  CHECK(QDir().mkpath(folder_a));
  CHECK(QDir().mkpath(folder_b));
  remove_test_scratch_dir(missing_folder);

  SettingsValueRestorer last_open_directory_restorer(QStringLiteral("lastOpenDirectory"));
  SettingsValueRestorer recent_folders_restorer(QStringLiteral("recentFolders"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("lastOpenDirectory"), folder_a);
    // Include a stale entry to confirm self-healing drops folders that no longer exist.
    settings.setValue(QStringLiteral("recentFolders"), QStringList{folder_a, missing_folder, folder_b});
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  auto* folders_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentFolderMenu"));
  CHECK(folders_menu != nullptr);
  const auto listed_folders = [folders_menu] { return recent_menu_paths(*folders_menu); };
  // The stale entry drops once the background existence check reports back.
  CHECK(process_events_until([&] { return listed_folders() == QStringList({folder_a, folder_b}); }));
  CHECK(folders_menu->actions().contains(require_action(window, "fileClearRecentFoldersAction")));

  // The Open dialog starts in the remembered directory.
  bool saw_open_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("openFileDialog")));
    CHECK(dialog != nullptr);
    CHECK(QFileInfo(dialog->directory().absolutePath()).absoluteFilePath() ==
          QFileInfo(folder_a).absoluteFilePath());
    saw_open_dialog = true;
    dialog->reject();
  });
  require_action(window, "fileOpenAction")->trigger();
  CHECK(saw_open_dialog);

  // Picking a recent folder opens the dialog pointed at that folder.
  auto* folder_list = folders_menu->findChild<QListWidget*>(QStringLiteral("fileOpenRecentFolderList"));
  CHECK(folder_list != nullptr);
  auto* folder_b_row = recent_menu_list_item(*folders_menu, folder_b);
  CHECK(folder_b_row != nullptr);
  const auto opened_in = answer_deferred_open_dialog([&] { emit folder_list->itemClicked(folder_b_row); });
  CHECK(opened_in == QFileInfo(folder_b).absoluteFilePath());

  // Clearing empties both the menu and the persisted list.
  require_action(window, "fileClearRecentFoldersAction")->trigger();
  QApplication::processEvents();
  CHECK(recent_menu_paths(*folders_menu).isEmpty());
  CHECK(!folders_menu->isEnabled());
  {
    auto settings = patchy::ui::app_settings();
    CHECK(settings.value(QStringLiteral("recentFolders")).toStringList().isEmpty());
  }
}

// Recent-history existence checks run on a worker and skip network paths: an
// unreachable share used to block startup and every File menu open for the SMB
// timeout. The unroutable TEST-NET host would hold a stat far past the wait
// below, so the missing local entry dropping in time proves the share was not
// stat'ed, and the share entry stays listed.
// The mount classifier behind is_network_recent_path on macOS and Linux: a
// synthetic table, so the result does not depend on the machine's mounts.
void ui_network_mount_classification_uses_deepest_mount_point() {
  using patchy::ui::MountEntry;
  const std::vector<MountEntry> mounts{
      {QStringLiteral("/"), QStringLiteral("apfs"), true},
      {QStringLiteral("/Volumes/share"), QStringLiteral("smbfs"), false},
      {QStringLiteral("/Volumes/share2"), QStringLiteral("apfs"), true},
      {QStringLiteral("/Volumes/share/nested-local"), QStringLiteral("apfs"), true},
      {QStringLiteral("/mnt/nas/"), QStringLiteral("nfs4"), true},  // Linux: the type decides
      {QStringLiteral("/run/user/1000/gvfs"), QStringLiteral("fuse.gvfsd-fuse"), true},
      {QStringLiteral("/net"), QStringLiteral("autofs"), true},
      {QStringLiteral("/media/usb"), QStringLiteral("fuse.ntfs-3g"), true},
  };
  const auto network = [&](const char* path) {
    return patchy::ui::path_is_on_network_mount(QString::fromUtf8(path), mounts);
  };
  CHECK(!network("/Users/seth/Pictures/a.psd"));
  CHECK(network("/Volumes/share/a.psd"));
  CHECK(network("/Volumes/share"));
  CHECK(!network("/Volumes/share2/a.psd"));   // sibling with a common prefix
  CHECK(!network("/Volumes/sharex/a.psd"));   // no component boundary match
  CHECK(!network("/Volumes/share/nested-local/a.psd"));  // deeper local mount wins
  CHECK(network("/mnt/nas/photos/a.psd"));    // trailing slash on the mount point
  CHECK(network("/run/user/1000/gvfs/smb-share:server=nas/a.psd"));
  CHECK(network("/net/box/a.psd"));           // automount trigger
  CHECK(!network("/media/usb/a.psd"));        // local FUSE stays local
  CHECK(!network(""));
  CHECK(!patchy::ui::path_is_on_network_mount(QStringLiteral("/anything"), {}));

  const auto* entry = patchy::ui::mount_for_path(QStringLiteral("/Volumes/share/x/y"), mounts);
  CHECK(entry != nullptr && entry->file_system == QStringLiteral("smbfs"));
  CHECK(patchy::ui::is_network_file_system_type(QStringLiteral("cifs")));
  CHECK(patchy::ui::is_network_file_system_type(QStringLiteral("fuse.sshfs")));
  CHECK(!patchy::ui::is_network_file_system_type(QStringLiteral("ext4")));
  CHECK(!patchy::ui::is_network_file_system_type(QStringLiteral("fuse.portal")));

  // /proc/self/mounts escapes spaces in mount points as \040.
  const auto parsed = patchy::ui::parse_proc_mounts(
      "proc /proc proc rw,nosuid 0 0\n"
      "/dev/sda1 / ext4 rw,relatime 0 0\n"
      "nas:/export /mnt/my\\040nas nfs4 rw,vers=4.2 0 0\n"
      "gvfsd-fuse /run/user/1000/gvfs fuse.gvfsd-fuse rw,nosuid,nodev,user_id=1000 0 0\n"
      "broken line\n");
  CHECK(parsed.size() == 4);
  if (parsed.size() == 4) {
    CHECK(parsed[1].mount_point == QStringLiteral("/"));
    CHECK(parsed[1].local);
    CHECK(parsed[2].mount_point == QStringLiteral("/mnt/my nas"));
    CHECK(parsed[2].file_system == QStringLiteral("nfs4"));
    CHECK(!parsed[2].local);
    CHECK(!parsed[3].local);
  }
  CHECK(patchy::ui::path_is_on_network_mount(QStringLiteral("/mnt/my nas/a.psd"), parsed));
  CHECK(!patchy::ui::path_is_on_network_mount(QStringLiteral("/home/x/a.psd"), parsed));

  // The live table never blocks and, off Windows, always knows the root.
  const auto live = patchy::ui::read_system_mounts();
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
  CHECK(!live.empty());
  CHECK(patchy::ui::mount_for_path(QStringLiteral("/"), live) != nullptr);
#else
  CHECK(live.empty());
#endif
}

void ui_recent_history_checks_in_background_and_skips_network_paths() {
  ensure_artifact_dir();
  const auto live_file = QFileInfo(QStringLiteral("test-artifacts/recent-bg-live.png")).absoluteFilePath();
  const auto missing_file = QFileInfo(QStringLiteral("test-artifacts/recent-bg-missing.png")).absoluteFilePath();
  const auto live_folder = QFileInfo(QStringLiteral("test-artifacts/recent-bg-dir")).absoluteFilePath();
  const auto missing_folder = QFileInfo(QStringLiteral("test-artifacts/recent-bg-dir-missing")).absoluteFilePath();
  const auto network_file = QStringLiteral("//192.0.2.1/share/recent-bg.psd");
  const auto network_folder = QStringLiteral("//192.0.2.1/share");
  {
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(QColor(60, 120, 180));
    CHECK(image.save(live_file));
  }
  QFile::remove(missing_file);
  CHECK(QDir().mkpath(live_folder));
  remove_test_scratch_dir(missing_folder);

  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  SettingsValueRestorer recent_folders_restorer(QStringLiteral("recentFolders"));
  const QStringList stored_files{network_file, missing_file, live_file};
  const QStringList stored_folders{network_folder, missing_folder, live_folder};
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), stored_files);
    settings.setValue(QStringLiteral("recentFolders"), stored_folders);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  auto* files_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentMenu"));
  auto* folders_menu = window.findChild<QMenu*>(QStringLiteral("fileOpenRecentFolderMenu"));
  CHECK(files_menu != nullptr);
  CHECK(folders_menu != nullptr);
  auto* file_menu = qobject_cast<QMenu*>(files_menu->parent());
  CHECK(file_menu != nullptr);
  const auto listed = [](QMenu* menu) { return recent_menu_paths(*menu); };

  CHECK(process_events_until([&] {
    return listed(files_menu) == QStringList({network_file, live_file}) &&
           listed(folders_menu) == QStringList({network_folder, live_folder});
  }));

  // Opening the File menu rereads the lists without touching the disk.
  QElapsedTimer timer;
  timer.start();
  emit file_menu->aboutToShow();
  CHECK(timer.elapsed() < 1000);
  CHECK(listed(files_menu) == QStringList({network_file, live_file}));
  CHECK(listed(folders_menu) == QStringList({network_folder, live_folder}));

  // Hidden entries stay stored, so an unplugged drive's entries come back.
  auto settings = patchy::ui::app_settings();
  settings.sync();
  CHECK(settings.value(QStringLiteral("recentFiles")).toStringList() == stored_files);
  CHECK(settings.value(QStringLiteral("recentFolders")).toStringList() == stored_folders);
}

void ui_open_dialog_hides_name_filter_details() {
  patchy::ui::MainWindow window;
  show_window(window);

  // The Open dialog lists one row per format so every supported filetype is readable;
  // the all-formats row alone summarizes (its ~50 patterns overflow the native
  // dropdown). The dialog runs with HideNameFilterDetails and each row embeds its
  // visible patterns in the display name ("TIFF Image (*.tif *.tiff)"): a "*." token
  // must survive stripping or the Windows 11 dialog re-appends the full pattern spec.
  bool saw_open_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("openFileDialog")));
    CHECK(dialog != nullptr);
    CHECK(dialog->testOption(QFileDialog::HideNameFilterDetails));
    const auto filters = dialog->nameFilters();
    CHECK(!filters.isEmpty());
    CHECK(filters.first().contains(QStringLiteral("*.psd")));
    CHECK(filters.first().contains(QStringLiteral("*.tif")));
    CHECK(filters.first().contains(QStringLiteral("*.jxl")));
    auto* type_combo = dialog->findChild<QComboBox*>(QStringLiteral("fileTypeCombo"));
    CHECK(type_combo != nullptr);
    QStringList displayed;
    for (int i = 0; i < type_combo->count(); ++i) {
      displayed.push_back(type_combo->itemText(i));
    }
    CHECK(displayed.size() > 10);
    CHECK(displayed.first().contains(QStringLiteral("*.psd")));
    CHECK(!displayed.first().contains(QStringLiteral("*.tif")));
    CHECK(displayed.contains(QStringLiteral("Photoshop Document (*.psd *.psb)")));
    CHECK(displayed.contains(QStringLiteral("TIFF Image (*.tif *.tiff)")));
    CHECK(displayed.contains(QStringLiteral("JPEG XL Image (*.jxl)")));
    CHECK(displayed.contains(QStringLiteral("Affinity Document (*.af *.afphoto *.afdesign *.afpub)")));
    CHECK(displayed.last() == QStringLiteral("All Files (*.*)"));
    saw_open_dialog = true;
    dialog->reject();
  });
  require_action(window, "fileOpenAction")->trigger();
  CHECK(saw_open_dialog);

  // Save As filters are one short entry per format; their extension details stay visible.
  bool saw_save_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("saveAsFileDialog")));
    CHECK(dialog != nullptr);
    CHECK(!dialog->testOption(QFileDialog::HideNameFilterDetails));
    saw_save_dialog = true;
    dialog->reject();
  });
  require_action(window, "fileSaveAsAction")->trigger();
  CHECK(saw_save_dialog);
}

void ui_open_dialog_opens_every_selected_file() {
  // File > Open is multi-select: every picked file opens as its own document in
  // dialog order and the last one ends up active, the same loop a multi-file
  // drop runs.
  ensure_artifact_dir();
  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  SettingsValueRestorer recent_folders_restorer(QStringLiteral("recentFolders"));
  SettingsValueRestorer last_open_directory_restorer(QStringLiteral("lastOpenDirectory"));
  const auto dir = QFileInfo(QStringLiteral("test-artifacts")).absoluteFilePath();
  const QStringList names{QStringLiteral("open-multi-a.png"), QStringLiteral("open-multi-b.png")};
  QStringList paths;
  for (const auto& name : names) {
    QImage image(48, 32, QImage::Format_RGB32);
    image.fill(QColor(90, 150, 210));
    paths.push_back(dir + QLatin1Char('/') + name);
    CHECK(image.save(paths.back()));
  }

  patchy::ui::MainWindow window;
  show_window(window);
  const auto sessions_before = patchy::ui::MainWindowTestAccess::session_count(window);
  QStringList picked;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("openFileDialog")));
    CHECK(dialog != nullptr);
    CHECK(dialog->fileMode() == QFileDialog::ExistingFiles);
    dialog->setDirectory(dir);
    // Qt's typed form for several files: "a" "b" (the widget dialog parses the quotes).
    auto* name_edit = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
    CHECK(name_edit != nullptr);
    name_edit->setText(QStringLiteral("\"%1\" \"%2\"").arg(names[0], names[1]));
    QApplication::processEvents();
    picked = dialog->selectedFiles();
    static_cast<QDialog*>(dialog)->accept();
  });
  require_action(window, "fileOpenAction")->trigger();
  QApplication::processEvents();

  CHECK(picked.size() == 2);
  CHECK(patchy::ui::MainWindowTestAccess::session_count(window) == sessions_before + 2);
  if (!picked.isEmpty()) {
    CHECK(QFileInfo(patchy::ui::MainWindowTestAccess::active_session_path(window)).absoluteFilePath() ==
          QFileInfo(picked.last()).absoluteFilePath());
  }
  // Every opened file went through the recent-files bookkeeping, not only the last.
  QStringList recent;
  {
    auto settings = patchy::ui::app_settings();
    settings.sync();
    for (const auto& entry : settings.value(QStringLiteral("recentFiles")).toStringList()) {
      recent.push_back(QFileInfo(entry).absoluteFilePath());
    }
  }
  for (const auto& path : paths) {
    CHECK(recent.contains(QFileInfo(path).absoluteFilePath()));
  }
}

QStringList top_level_menu_texts(QMenuBar& menu_bar) {
  QStringList texts;
  for (auto* action : menu_bar.actions()) {
    texts << action->text().remove('&');
  }
  return texts;
}

void choose_preferences_language(patchy::ui::MainWindow& window, const QString& language_code) {
  auto* preferences = require_action(window, "filePreferencesAction");
  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesLanguageCombo"));
    CHECK(combo != nullptr);
    const auto index = combo->findData(language_code);
    CHECK(index >= 0);
    combo->setCurrentIndex(index);
    saw_dialog = true;
    dialog->accept();
  });
  preferences->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
}

void update_manifest_parser_handles_supported_cases() {
  const QByteArray newer_manifest = R"({
    "platforms": {
      "windows": {
        "version": "0.2",
        "download_url": "https://rtsoft.com/patchy/PatchyWindowsInstaller.exe"
      },
      "macos": {
        "version": "0.3.0",
        "download_url": "https://rtsoft.com/patchy/PatchyMacOS.dmg"
      }
    }
  })";
  const auto update = patchy::ui::parse_update_manifest(newer_manifest, QStringLiteral("windows"),
                                                        QStringLiteral("0.1.0"));
  CHECK(update.has_value());
  CHECK(update->platform == QStringLiteral("windows"));
  CHECK(update->version == QStringLiteral("0.2"));
  CHECK(update->download_url == QUrl(QStringLiteral("https://rtsoft.com/patchy/PatchyWindowsInstaller.exe")));
  const auto update_result =
      patchy::ui::inspect_update_manifest(newer_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"));
  CHECK(update_result.status == patchy::ui::UpdateCheckStatus::UpdateAvailable);
  CHECK(update_result.update.has_value());
  CHECK(update_result.latest_version == QStringLiteral("0.2"));
  CHECK(!patchy::ui::update_version_is_newer(QStringLiteral("0.2.0"), QStringLiteral("0.2")));
  CHECK(!patchy::ui::update_version_is_newer(QStringLiteral("0.2"), QStringLiteral("0.2.0")));
  CHECK(patchy::ui::update_version_is_newer(QStringLiteral("0.10"), QStringLiteral("0.2")));
  CHECK(!patchy::ui::update_version_is_newer(QStringLiteral("0.1.0"), QStringLiteral("0.1.0")));
  CHECK(!patchy::ui::update_version_is_newer(QStringLiteral("0.0.9"), QStringLiteral("0.1.0")));

  const QByteArray equal_manifest = R"({
    "platforms": {
      "windows": {
        "version": "0.1.0",
        "download_url": "https://rtsoft.com/patchy/PatchyWindowsInstaller.exe"
      }
    }
  })";
  CHECK(!patchy::ui::parse_update_manifest(equal_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"))
             .has_value());
  const auto equal_result =
      patchy::ui::inspect_update_manifest(equal_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"));
  CHECK(equal_result.status == patchy::ui::UpdateCheckStatus::NoUpdateAvailable);
  CHECK(equal_result.latest_version == QStringLiteral("0.1.0"));
  const QByteArray lower_manifest = R"({
    "platforms": {
      "windows": {
        "version": "0.0.9",
        "download_url": "https://rtsoft.com/patchy/PatchyWindowsInstaller.exe"
      }
    }
  })";
  CHECK(!patchy::ui::parse_update_manifest(lower_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"))
             .has_value());
  CHECK(!patchy::ui::parse_update_manifest(newer_manifest, QStringLiteral("linux"), QStringLiteral("0.1.0"))
             .has_value());
  const auto missing_platform_result =
      patchy::ui::inspect_update_manifest(newer_manifest, QStringLiteral("linux"), QStringLiteral("0.1.0"));
  CHECK(missing_platform_result.status == patchy::ui::UpdateCheckStatus::MissingPlatform);
  // A manifest that does carry a linux entry parses for the linux platform id.
  const QByteArray linux_manifest = R"({
    "platforms": {
      "linux": {
        "version": "0.2",
        "download_url": "https://rtsoft.com/patchy/Patchy.flatpak"
      }
    }
  })";
  const auto linux_update =
      patchy::ui::parse_update_manifest(linux_manifest, QStringLiteral("linux"), QStringLiteral("0.1.0"));
  CHECK(linux_update.has_value());
  CHECK(linux_update->platform == QStringLiteral("linux"));
  CHECK(linux_update->download_url == QUrl(QStringLiteral("https://rtsoft.com/patchy/Patchy.flatpak")));
  CHECK(linux_update->appimage_url.isEmpty());
  // The optional appimage_url (a permanent manifest key) is what an AppImage copy downloads;
  // a malformed one is dropped and the update is still offered.
  const auto with_appimage_url = [](const QString& value) {
    return patchy::ui::parse_update_manifest(QStringLiteral(R"({"platforms": {"linux": {"version": "0.2",
        "download_url": "https://rtsoft.com/patchy/Patchy.flatpak", "appimage_url": %1}}})")
                                                 .arg(value)
                                                 .toUtf8(),
                                             QStringLiteral("linux"), QStringLiteral("0.1.0"));
  };
  const auto appimage_update = with_appimage_url(QStringLiteral("\"https://rtsoft.com/patchy/Patchy.AppImage\""));
  CHECK(appimage_update.has_value());
  CHECK(appimage_update->appimage_url == QUrl(QStringLiteral("https://rtsoft.com/patchy/Patchy.AppImage")));
  const auto bad_appimage_update = with_appimage_url(QStringLiteral("\"not a url\""));
  CHECK(bad_appimage_update.has_value() && bad_appimage_update->appimage_url.isEmpty());

  // The AppImage an AppImage copy installs by itself (permanent keys appimage_install_url,
  // _size, _sha256 and appimage_min_version): all three or none, https, and only from this
  // repository's GitHub releases, since no publisher signature backs it.
  const QString appimage_sha(64, QLatin1Char('c'));
  const auto with_appimage_install = [&appimage_sha](const QString& url, const QString& extra = {}) {
    return patchy::ui::parse_update_manifest(
        QStringLiteral(R"({"platforms": {"linux": {"version": "0.2", "download_url": "https://rtsoft.com/p.flatpak",
            "appimage_install_url": "%1", "appimage_install_size": 1000, "appimage_install_sha256": "%2"%3}}})")
            .arg(url, appimage_sha, extra)
            .toUtf8(),
        QStringLiteral("linux"), QStringLiteral("0.1.0"));
  };
  const auto pinned_release =
      QStringLiteral("https://github.com/SethRobinson/Patchy/releases/download/v0.2/PatchyLinux.AppImage");
  const auto installable = with_appimage_install(pinned_release);
  CHECK(installable.has_value() && patchy::ui::update_has_appimage_install(*installable));
  CHECK(installable.has_value() && installable->appimage_install_url == QUrl(pinned_release));
  CHECK(installable.has_value() && installable->appimage_install_size == 1000);
  CHECK(installable.has_value() && installable->appimage_install_sha256 == appimage_sha.toLatin1());
  CHECK(installable.has_value() && !patchy::ui::update_has_installer(*installable));
  const auto elsewhere = with_appimage_install(QStringLiteral("https://example.com/PatchyLinux.AppImage"));
  CHECK(elsewhere.has_value() && !patchy::ui::update_has_appimage_install(*elsewhere));
  const auto other_repo = with_appimage_install(
      QStringLiteral("https://github.com/someone/Patchy/releases/download/v0.2/PatchyLinux.AppImage"));
  CHECK(other_repo.has_value() && !patchy::ui::update_has_appimage_install(*other_repo));
  const auto switched_off = with_appimage_install(pinned_release, QStringLiteral(R"(, "appimage_min_version": "0.2")"));
  CHECK(switched_off.has_value() && !patchy::ui::update_has_appimage_install(*switched_off));
  const auto still_on = with_appimage_install(pinned_release, QStringLiteral(R"(, "appimage_min_version": "0.1")"));
  CHECK(still_on.has_value() && patchy::ui::update_has_appimage_install(*still_on));
  CHECK(linux_update.has_value() && !patchy::ui::update_has_appimage_install(*linux_update));
  const auto invalid_manifest_result =
      patchy::ui::inspect_update_manifest(QByteArray("{"), QStringLiteral("windows"), QStringLiteral("0.1.0"));
  CHECK(invalid_manifest_result.status == patchy::ui::UpdateCheckStatus::InvalidManifest);
  CHECK(!patchy::ui::parse_update_manifest(QByteArray("{"), QStringLiteral("windows"), QStringLiteral("0.1.0"))
             .has_value());

  const QByteArray empty_url_manifest = R"({
    "platforms": {
      "windows": {
        "version": "0.2.0",
        "download_url": ""
      }
    }
  })";
  CHECK(!patchy::ui::parse_update_manifest(empty_url_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"))
             .has_value());
  const auto empty_url_result =
      patchy::ui::inspect_update_manifest(empty_url_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"));
  CHECK(empty_url_result.status == patchy::ui::UpdateCheckStatus::InvalidDownloadUrl);

  const QByteArray relative_url_manifest = R"({
    "platforms": {
      "windows": {
        "version": "0.2.0",
        "download_url": "PatchyWindowsInstaller.exe"
      }
    }
  })";
  CHECK(!patchy::ui::parse_update_manifest(relative_url_manifest, QStringLiteral("windows"), QStringLiteral("0.1.0"))
             .has_value());

  // The one-click installer fields (docs/auto-update.md) are all or nothing: an entry
  // without them, with one missing, with a non-https URL or with a malformed hash is still
  // an update, offered as a browser download.
  CHECK(!patchy::ui::update_has_installer(*update));
  const auto installer_manifest = [](const QString& extra_fields) {
    return QStringLiteral(R"({"platforms": {"windows": {"version": "0.2",
        "download_url": "https://rtsoft.com/patchy/PatchyWindowsInstaller.exe"%1}}})")
        .arg(extra_fields)
        .toUtf8();
  };
  const auto sha = QStringLiteral("0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef");
  const auto with_installer = patchy::ui::parse_update_manifest(
      installer_manifest(QStringLiteral(R"(, "installer_url": "https://rtsoft.com/v0.2/PatchyWindowsInstaller.exe",
                                           "installer_size": 123456789, "installer_sha256": "%1")")
                             .arg(sha)),
      QStringLiteral("windows"), QStringLiteral("0.1.0"));
  CHECK(with_installer.has_value());
  if (with_installer.has_value()) {
    CHECK(patchy::ui::update_has_installer(*with_installer));
    CHECK(with_installer->installer_url == QUrl(QStringLiteral("https://rtsoft.com/v0.2/PatchyWindowsInstaller.exe")));
    CHECK(with_installer->installer_size == 123456789);
    CHECK(with_installer->installer_sha256 == sha.toLower().toLatin1());
    CHECK(with_installer->download_url == QUrl(QStringLiteral("https://rtsoft.com/patchy/PatchyWindowsInstaller.exe")));
  }
  for (const auto& fields : {
           QStringLiteral(R"(, "installer_url": "https://rtsoft.com/i.exe", "installer_size": 10)"),
           QStringLiteral(R"(, "installer_url": "https://rtsoft.com/i.exe", "installer_sha256": "%1")").arg(sha),
           QStringLiteral(R"(, "installer_size": 10, "installer_sha256": "%1")").arg(sha),
           QStringLiteral(R"(, "installer_url": "http://rtsoft.com/i.exe", "installer_size": 10,
                             "installer_sha256": "%1")").arg(sha),
           QStringLiteral(R"(, "installer_url": "https://rtsoft.com/i.exe", "installer_size": 0,
                             "installer_sha256": "%1")").arg(sha),
           QStringLiteral(R"(, "installer_url": "https://rtsoft.com/i.exe", "installer_size": 10,
                             "installer_sha256": "not-a-hash")"),
       }) {
    const auto partial =
        patchy::ui::parse_update_manifest(installer_manifest(fields), QStringLiteral("windows"), QStringLiteral("0.1.0"));
    CHECK(partial.has_value());
    CHECK(partial.has_value() && !patchy::ui::update_has_installer(*partial));
  }
  // The remote off switch: installer_min_version keeps copies older than it on the browser
  // download (a release whose self-update is broken), while newer ones still install by
  // themselves. A value that does not parse switches self-install off for everyone.
  const auto with_min_version = [&](const QString& min_version, const QString& running) {
    const auto parsed = patchy::ui::parse_update_manifest(
        installer_manifest(QStringLiteral(R"(, "installer_url": "https://rtsoft.com/v9/PatchyWindowsInstaller.exe",
                                             "installer_size": 10, "installer_sha256": "%1",
                                             "installer_min_version": %2)")
                               .arg(sha, min_version))
            .replace("\"version\": \"0.2\"", "\"version\": \"9.0\""),
        QStringLiteral("windows"), running);
    CHECK(parsed.has_value());  // still an update, whatever the switch says
    return parsed.has_value() && patchy::ui::update_has_installer(*parsed);
  };
  CHECK(!with_min_version(QStringLiteral("\"1.11\""), QStringLiteral("1.10")));
  CHECK(!with_min_version(QStringLiteral("\"1.11\""), QStringLiteral("1.9")));
  CHECK(with_min_version(QStringLiteral("\"1.11\""), QStringLiteral("1.11")));
  CHECK(with_min_version(QStringLiteral("\"1.11\""), QStringLiteral("1.12")));
  CHECK(with_min_version(QStringLiteral("\"1.10\""), QStringLiteral("1.10")));
  CHECK(!with_min_version(QStringLiteral("\"never\""), QStringLiteral("1.10")));
  CHECK(!with_min_version(QStringLiteral("\"\""), QStringLiteral("1.10")));
  CHECK(!with_min_version(QStringLiteral("111"), QStringLiteral("1.10")));  // a number, not a version string

  // A manifest chosen through PATCHY_UPDATE_MANIFEST_URL may name an http installer (a
  // local trial of the whole update); the shipped manifest never can (the fourth case above).
  {
    const auto previous = qgetenv("PATCHY_UPDATE_MANIFEST_URL");
    qputenv("PATCHY_UPDATE_MANIFEST_URL", "http://127.0.0.1:1/latest_version.json");
    const auto local = patchy::ui::parse_update_manifest(
        installer_manifest(QStringLiteral(R"(, "installer_url": "http://127.0.0.1:1/i.exe", "installer_size": 10,
                                             "installer_sha256": "%1")")
                               .arg(sha)),
        QStringLiteral("windows"), QStringLiteral("0.1.0"));
    if (previous.isEmpty()) {
      qunsetenv("PATCHY_UPDATE_MANIFEST_URL");
    } else {
      qputenv("PATCHY_UPDATE_MANIFEST_URL", previous);
    }
    CHECK(local.has_value() && patchy::ui::update_has_installer(*local));
  }
}

// Only a copy the Windows installer made updates itself: the uninstall entry's
// InstallLocation must name the folder the app runs from.
void update_install_location_matching_ignores_case_and_separators() {
  using patchy::ui::install_location_matches;
  CHECK(install_location_matches(QStringLiteral("C:\\Users\\Me\\AppData\\Local\\Programs\\Patchy"),
                                 QStringLiteral("C:/Users/Me/AppData/Local/Programs/Patchy")));
  CHECK(install_location_matches(QStringLiteral("c:\\apps\\patchy\\"), QStringLiteral("C:/Apps/Patchy")));
  CHECK(!install_location_matches(QStringLiteral("C:\\Apps\\Patchy"), QStringLiteral("C:/Apps/Patchy 2")));
  CHECK(!install_location_matches(QStringLiteral("C:\\Apps"), QStringLiteral("C:/Apps/Patchy")));
  CHECK(!install_location_matches(QString(), QStringLiteral("C:/Apps/Patchy")));
  CHECK(!install_location_matches(QStringLiteral("C:\\Apps\\Patchy"), QString()));
  // The suites never run from an installed copy, so they never start a real update.
  CHECK(!patchy::ui::installed_copy_can_self_update());
  CHECK(patchy::ui::update_download_folder(QStringLiteral("1.10")).endsWith(QStringLiteral("/patchy-update-1.10")));
  CHECK(patchy::ui::update_download_folder(QStringLiteral("..\\1.10/x")).endsWith(QStringLiteral("/patchy-update-..1.10")));
}

namespace {

// Scoped PATCHY_UPDATE_MANIFEST_URL override for the request tests below.
class UpdateManifestUrlOverride {
 public:
  explicit UpdateManifestUrlOverride(const QString& url) : previous_(qgetenv("PATCHY_UPDATE_MANIFEST_URL")) {
    qputenv("PATCHY_UPDATE_MANIFEST_URL", url.toUtf8());
  }
  ~UpdateManifestUrlOverride() {
    if (previous_.isEmpty()) {
      qunsetenv("PATCHY_UPDATE_MANIFEST_URL");
    } else {
      qputenv("PATCHY_UPDATE_MANIFEST_URL", previous_);
    }
  }
  UpdateManifestUrlOverride(const UpdateManifestUrlOverride&) = delete;
  UpdateManifestUrlOverride& operator=(const UpdateManifestUrlOverride&) = delete;

 private:
  QByteArray previous_;
};

// A one-shot HTTP server on the loopback interface that answers every request
// with the given manifest body.
class ManifestServer {
 public:
  explicit ManifestServer(QByteArray body) : body_(std::move(body)) {
    CHECK(server_.listen(QHostAddress::LocalHost));
    QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this] {
      while (auto* socket = server_.nextPendingConnection()) {
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
          if (!socket->readAll().contains("\r\n\r\n")) {
            return;
          }
          ++requests_;
          socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                        QByteArray::number(body_.size()) + "\r\n\r\n" + body_);
          socket->disconnectFromHost();
        });
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      }
    });
  }
  [[nodiscard]] QString manifest_url() const {
    return QStringLiteral("http://127.0.0.1:%1/latest_version.json").arg(server_.serverPort());
  }
  [[nodiscard]] int requests() const { return requests_; }

 private:
  QTcpServer server_;
  QByteArray body_;
  int requests_{0};
};

}  // namespace

// main() waits a bounded time for tracked workers at quit and force-exits when
// one is still blocked in the OS: the timed wait must report a running worker,
// then succeed once it finishes, and the count must balance.
void ui_background_worker_wait_is_bounded() {
  std::atomic<bool> release{false};
  std::atomic<bool> finished{false};
  const int before = patchy::ui::tracked_background_worker_count();
  patchy::ui::run_tracked_background_worker([&release, &finished] {
    while (!release.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    finished = true;
  });
  if (patchy::ui::kBackgroundWorkRunsInline) {
    // Ran inline (single-threaded wasm): the worker could not have blocked.
    CHECK(patchy::ui::wait_for_tracked_background_workers(std::chrono::milliseconds(0)));
    return;
  }
  CHECK(patchy::ui::tracked_background_worker_count() == before + 1);
  QElapsedTimer timer;
  timer.start();
  CHECK(!patchy::ui::wait_for_tracked_background_workers(std::chrono::milliseconds(150)));
  CHECK(timer.elapsed() >= 100);
  CHECK(!finished.load());
  release = true;
  CHECK(patchy::ui::wait_for_tracked_background_workers(std::chrono::seconds(10)));
  CHECK(finished.load());
  CHECK(patchy::ui::tracked_background_worker_count() == before);
}

void ui_update_manifest_url_honors_environment_override() {
  {
    const UpdateManifestUrlOverride override(QStringLiteral("http://127.0.0.1:1/custom.json"));
    CHECK(patchy::ui::update_manifest_url() == QUrl(QStringLiteral("http://127.0.0.1:1/custom.json")));
  }
  CHECK(patchy::ui::update_manifest_url().host() == QStringLiteral("raw.githubusercontent.com"));
}

// The request resolves its host on a detached thread and only then talks to the
// server (GitHub issue 48): the whole path, resolver included, must deliver the
// manifest to the owner's thread.
void ui_update_check_fetches_manifest_after_resolving_host() {
  ManifestServer server(R"({"platforms": {"windows": {"version": "9.9", "download_url": "https://rtsoft.com/w.exe"},
                                          "macos": {"version": "9.9", "download_url": "https://rtsoft.com/m.dmg"},
                                          "linux": {"version": "9.9", "download_url": "https://rtsoft.com/l.flatpak"}}})");
  const UpdateManifestUrlOverride override(server.manifest_url());
  QObject owner;
  std::optional<patchy::ui::UpdateCheckResult> result;
  patchy::ui::request_update_check(&owner, QStringLiteral("0.1.0"),
                                   [&result](patchy::ui::UpdateCheckResult value) { result = std::move(value); });
  CHECK(process_events_until([&] { return result.has_value(); }, 15000));
  CHECK(result.has_value());
  if (result.has_value()) {
    CHECK(result->status == patchy::ui::UpdateCheckStatus::UpdateAvailable);
    CHECK(result->latest_version == QStringLiteral("9.9"));
  }
  CHECK(server.requests() == 1);
}

// An unresolvable host fails the check without a request, and an owner destroyed
// while the resolver is still out never hears back (no dangling post at quit).
void ui_update_check_reports_unresolvable_host_and_drops_destroyed_owner() {
  const UpdateManifestUrlOverride override(QStringLiteral("https://patchy-no-such-host.invalid/latest_version.json"));
  {
    QObject owner;
    std::optional<patchy::ui::UpdateCheckResult> result;
    patchy::ui::request_update_check(&owner, QStringLiteral("0.1.0"),
                                     [&result](patchy::ui::UpdateCheckResult value) { result = std::move(value); });
    CHECK(process_events_until([&] { return result.has_value(); }, 15000));
    if (result.has_value()) {
      CHECK(result->status == patchy::ui::UpdateCheckStatus::NetworkError);
      CHECK(result->http_status == 0);
    }
  }
  {
    bool called = false;
    {
      QObject owner;
      patchy::ui::request_update_check(&owner, QStringLiteral("0.1.0"),
                                       [&called](patchy::ui::UpdateCheckResult) { called = true; });
    }
    process_events_for(500);
    CHECK(!called);
  }
}

void ui_update_available_dialog_warns_to_close_patchy_before_installing() {
  patchy::ui::MainWindow window;
  show_window(window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")));
    CHECK(dialog != nullptr);
    // The install advice is per-platform (installer exe / DMG / Flatpak update).
#if defined(Q_OS_MACOS)
    CHECK(dialog->text().contains(QStringLiteral("drag the new Patchy into Applications")));
#elif defined(Q_OS_LINUX)
    // Installs update from the Patchy Flatpak repository (GitHub issue 28), so the
    // advice is one flatpak command: no bundle download, and --user like every
    // documented install command (no root, GitHub issue 14).
    CHECK(dialog->text().contains(QStringLiteral("flatpak update --user -y com.rtsoft.patchy")));
    CHECK(!dialog->text().contains(QStringLiteral("curl ")));
    CHECK(dialog->findChild<QAbstractButton*>(QStringLiteral("updateCopyCommandButton")) != nullptr);
#else
    CHECK(dialog->text().contains(
        QStringLiteral("Save your work and close Patchy before running the installer.")));
#endif
    // No pinned installer in this update, and the suite is not an installed copy.
    CHECK(dialog->findChild<QAbstractButton*>(QStringLiteral("updateInstallNowButton")) == nullptr);
    saw_dialog = true;
    dialog->reject();
  });

  // Each platform gets its real download URL (the GitHub latest-release permalink that
  // latest_version.json carries), which is what the Download button opens.
#if defined(Q_OS_LINUX)
  const QUrl download_url(
      QStringLiteral("https://github.com/SethRobinson/Patchy/releases/latest/download/PatchyLinux.flatpak"));
#else
  const QUrl download_url(
      QStringLiteral("https://github.com/SethRobinson/Patchy/releases/latest/download/PatchyWindowsInstaller.exe"));
#endif
  window.show_update_available({QStringLiteral("windows"), QStringLiteral("9.9"), download_url});
  CHECK(saw_dialog);
}

namespace {

// Scoped APPIMAGE/APPDIR for the AppImage tests: what the AppImage runtime sets.
class AppImageEnvironment {
 public:
  AppImageEnvironment(const QByteArray& appimage, const QByteArray& appdir)
      : previous_appimage_(qgetenv("APPIMAGE")), previous_appdir_(qgetenv("APPDIR")),
        had_appimage_(qEnvironmentVariableIsSet("APPIMAGE")), had_appdir_(qEnvironmentVariableIsSet("APPDIR")) {
    qputenv("APPIMAGE", appimage);
    qputenv("APPDIR", appdir);
  }
  ~AppImageEnvironment() {
    restore("APPIMAGE", had_appimage_, previous_appimage_);
    restore("APPDIR", had_appdir_, previous_appdir_);
  }
  AppImageEnvironment(const AppImageEnvironment&) = delete;
  AppImageEnvironment& operator=(const AppImageEnvironment&) = delete;

 private:
  static void restore(const char* name, bool had, const QByteArray& value) {
    if (had) {
      qputenv(name, value);
    } else {
      qunsetenv(name);
    }
  }
  QByteArray previous_appimage_;
  QByteArray previous_appdir_;
  bool had_appimage_;
  bool had_appdir_;
};

// An APPDIR that contains this test binary, as the AppImage mount contains patchy.
QByteArray appdir_containing_this_binary() {
  auto dir = QDir(QCoreApplication::applicationDirPath());
  dir.cdUp();
  return dir.absolutePath().toUtf8();
}

}  // namespace

// An AppImage runs from a new /tmp/.mount_* directory each launch (docs/appimage.md), so
// the user-visible path is the .AppImage file, and only when this executable really lives
// in the mount: an AppImage'd program that starts a native Patchy leaves its own APPIMAGE
// and APPDIR behind. The attach socket then stops depending on the mount, or the user's
// Patchy and an assistant's `<file> patchy-mcp --attach` would never meet.
void appimage_environment_names_the_appimage_file() {
  CHECK(patchy::ui::appimage_file_path().isEmpty());
  CHECK(patchy::ui::user_visible_executable_path() == QCoreApplication::applicationFilePath());
  const auto plain_endpoint = patchy::ui::mcp_attachment_endpoint();
  {
    const AppImageEnvironment env("/home/someone/Apps/PatchyLinux.AppImage", appdir_containing_this_binary());
#ifdef Q_OS_LINUX
    CHECK(patchy::ui::appimage_file_path() == QStringLiteral("/home/someone/Apps/PatchyLinux.AppImage"));
    CHECK(patchy::ui::user_visible_executable_path() == QStringLiteral("/home/someone/Apps/PatchyLinux.AppImage"));
    const auto identity = (QDir::homePath() + QStringLiteral("\nappimage")).toUtf8();
    CHECK(patchy::ui::mcp_attachment_endpoint() ==
          QStringLiteral("PatchyMcp-") +
              QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(32)));
    CHECK(patchy::ui::mcp_attachment_endpoint() != plain_endpoint);
    patchy::ui::AiControlPaths paths = patchy::ui::resolve_ai_control_paths();
    CHECK(paths.appimage_path == QStringLiteral("/home/someone/Apps/PatchyLinux.AppImage"));
#else
    // Windows and macOS have no AppImages; a stray APPIMAGE changes nothing.
    CHECK(patchy::ui::appimage_file_path().isEmpty());
    CHECK(patchy::ui::mcp_attachment_endpoint() == plain_endpoint);
#endif
  }
  {
    // Another program's AppImage: this binary is not under its APPDIR.
    QTemporaryDir elsewhere;
    CHECK(elsewhere.isValid());
    const AppImageEnvironment env("/home/someone/Apps/Other.AppImage", elsewhere.path().toUtf8());
    CHECK(patchy::ui::appimage_file_path().isEmpty());
    CHECK(patchy::ui::mcp_attachment_endpoint() == plain_endpoint);
  }
  {
    const AppImageEnvironment env("relative/PatchyLinux.AppImage", appdir_containing_this_binary());
    CHECK(patchy::ui::appimage_file_path().isEmpty());
  }
}

// An AppImage copy is one file the user replaces: its update prompt names that file, opens
// the AppImage download (the manifest's appimage_url, else the GitHub permalink), and
// offers no flatpak command.
void ui_update_available_dialog_offers_appimage_download() {
#if defined(Q_OS_LINUX)
  const AppImageEnvironment env("/home/someone/Apps/PatchyLinux.AppImage", appdir_containing_this_binary());
  patchy::ui::MainWindow window;
  show_window(window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")));
    CHECK(dialog != nullptr);
    CHECK(dialog->text().contains(QStringLiteral("replace this file")));
    CHECK(dialog->text().contains(QStringLiteral("/home/someone/Apps/PatchyLinux.AppImage")));
    CHECK(!dialog->text().contains(QStringLiteral("flatpak")));
    CHECK(dialog->findChild<QAbstractButton*>(QStringLiteral("updateCopyCommandButton")) == nullptr);
    CHECK(dialog->defaultButton() != nullptr &&
          dialog->defaultButton()->objectName() == QStringLiteral("updateDownloadButton"));
    saw_dialog = true;
    dialog->reject();
  });
  window.show_update_available({QStringLiteral("linux"), QStringLiteral("9.9"),
                                QUrl(QStringLiteral(
                                    "https://github.com/SethRobinson/Patchy/releases/latest/download/PatchyLinux.flatpak"))});
  CHECK(saw_dialog);
#endif
}

// One-click update (docs/auto-update.md), Windows only: an installed copy offered an update
// with a pinned installer gets "Update Now"; without either, the browser download stays.
void ui_update_available_dialog_offers_one_click_update_for_installed_copy() {
#if defined(Q_OS_WIN)
  patchy::ui::MainWindow window;
  show_window(window);

  patchy::ui::UpdateInfo update{QStringLiteral("windows"), QStringLiteral("9.9"),
                                QUrl(QStringLiteral("https://rtsoft.com/patchy/PatchyWindowsInstaller.exe"))};
  update.installer_url = QUrl(QStringLiteral("https://rtsoft.com/patchy/v9.9/PatchyWindowsInstaller.exe"));
  update.installer_size = 1000;
  update.installer_sha256 = QByteArray(64, 'a');

  const auto shown_with_update_now = [&window](const patchy::ui::UpdateInfo& info, QString* text) {
    bool saw_dialog = false;
    bool update_now = false;
    QTimer::singleShot(0, [&] {
      auto* dialog = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")));
      CHECK(dialog != nullptr);
      auto* button = dialog->findChild<QAbstractButton*>(QStringLiteral("updateInstallNowButton"));
      update_now = button != nullptr;
      if (button != nullptr) {
        CHECK(dialog->defaultButton() == button);
      }
      if (text != nullptr) {
        *text = dialog->text();
      }
      saw_dialog = true;
      dialog->reject();  // never an accept: that would start the download
    });
    window.show_update_available(info);
    CHECK(saw_dialog);
    return update_now;
  };

  QString text;
  patchy::ui::MainWindowTestAccess::set_self_update_override(window, true);
  CHECK(shown_with_update_now(update, &text));
  CHECK(text.contains(QStringLiteral("Patchy can download the update, close, install it, and start again.")));
  CHECK(!text.contains(QStringLiteral("before running the installer")));

  // The portable zip or a development build: the browser download.
  patchy::ui::MainWindowTestAccess::set_self_update_override(window, false);
  CHECK(!shown_with_update_now(update, &text));
  CHECK(text.contains(QStringLiteral("before running the installer")));

  // An installed copy and a manifest without the installer fields: the same.
  patchy::ui::MainWindowTestAccess::set_self_update_override(window, true);
  CHECK(!shown_with_update_now({QStringLiteral("windows"), QStringLiteral("9.9"), update.download_url}, nullptr));
  CHECK(!patchy::ui::MainWindowTestAccess::update_install_pending(window));
#endif
}

// After a one-click update the installer hands Patchy the AI apps still running the
// previous connector; the note names them and the version now installed.
void ui_update_connector_restart_notice_names_the_apps() {
  patchy::ui::MainWindow window;
  show_window(window);
  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog =
        qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateConnectorNoticeMessageBox")));
    CHECK(dialog != nullptr);
    if (dialog != nullptr) {
      CHECK(dialog->text().contains(QStringLiteral("Claude, Codex")));
      CHECK(dialog->text().contains(QStringLiteral(PATCHY_VERSION)));
      CHECK(dialog->text().contains(QStringLiteral("Restart them")));
      saw_dialog = true;
      dialog->accept();
    }
  });
  window.show_connector_restart_notice({QStringLiteral("Claude"), QStringLiteral("Codex")});
  CHECK(saw_dialog);
  // Nothing to name: no dialog at all.
  window.show_connector_restart_notice({});
  CHECK(find_top_level_dialog(QStringLiteral("updateConnectorNoticeMessageBox")) == nullptr);
}

// A connector an update left behind notices that another file now sits at its path
// (ui/connector_freshness.hpp); one that is still current adds nothing to its replies.
void connector_update_notice_reports_a_replaced_executable() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const auto path = dir.filePath(QStringLiteral("patchy-mcp.exe"));
  const auto write = [&path](const QByteArray& bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size());
  };
  write("version one");
  const patchy::ui::ReplacedFileWatch watch(path);
  CHECK(!watch.replaced());
  // The update: the running file is renamed aside and a new one takes its name.
  CHECK(QFile::rename(path, path + QStringLiteral(".old-20260101000000")));
  CHECK(watch.replaced());
  write("version two, a different size");
  CHECK(watch.replaced());
  // A path with no file at the start never reports a replacement.
  const patchy::ui::ReplacedFileWatch absent(dir.filePath(QStringLiteral("absent.exe")));
  CHECK(!absent.replaced());

  // This test binary has not been replaced: no notice, and replies stay as they were.
  patchy::ui::watch_running_executable();
  CHECK(patchy::ui::connector_update_notice().isEmpty());
  QJsonObject structured{{QStringLiteral("version"), QStringLiteral("1")}};
  QJsonArray content;
  CHECK(!patchy::ui::add_connector_update_notice(structured, content));
  CHECK(structured.size() == 1 && content.isEmpty());

  // The connector's own update check words its finding for the assistant; nothing is
  // reported until a check has found a newer version.
  CHECK(patchy::ui::connector_update_available().isEmpty());
  const auto available = patchy::ui::connector_update_available_message(QStringLiteral("9.9"));
  CHECK(available.contains(QStringLiteral("Patchy 9.9 is available")));
  // The address is fixed in the code: nothing a manifest says reaches the assistant as text.
  CHECK(available.contains(QStringLiteral("https://github.com/SethRobinson/Patchy/releases/latest")));
  CHECK(available.contains(QStringLiteral("open the Patchy app")));
}

// The installer is checked again at the moment it is started: its pinned checksum, and for a
// signed Patchy the same Authenticode publisher. A file that fails either is never run.
void update_installer_is_checked_again_before_it_runs() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const auto path = dir.filePath(QStringLiteral("PatchyWindowsInstaller.exe"));
  const QByteArray bytes(50000, 'i');
  {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size());
  }
  const auto sha = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
  CHECK(patchy::ui::file_matches_checksum(path, bytes.size(), sha));
  CHECK(patchy::ui::file_matches_checksum(path, bytes.size(), sha.toUpper()));
  CHECK(!patchy::ui::file_matches_checksum(path, bytes.size() + 1, sha));
  CHECK(!patchy::ui::file_matches_checksum(path, bytes.size(), QByteArray(64, '0')));
  CHECK(!patchy::ui::file_matches_checksum(path, 0, {}));
  CHECK(!patchy::ui::file_matches_checksum(dir.filePath(QStringLiteral("absent.exe")), bytes.size(), sha));

  using patchy::ui::FileSignature;
  using patchy::ui::installer_signer_acceptable;
  const FileSignature unsigned_file{};
  const FileSignature publisher{true, QStringLiteral("Seth A. Robinson")};
  const FileSignature someone_else{true, QStringLiteral("Someone Else")};
  CHECK(installer_signer_acceptable(publisher, publisher));
  CHECK(!installer_signer_acceptable(publisher, someone_else));
  CHECK(!installer_signer_acceptable(publisher, unsigned_file));
  // A development build has no publisher to compare with; the checksum still applies.
  CHECK(installer_signer_acceptable(unsigned_file, unsigned_file));
  CHECK(installer_signer_acceptable(unsigned_file, someone_else));

  // A swapped or damaged file is refused before anything is started.
  patchy::ui::UpdateInstallerLaunch launch{path, dir.path(), QCoreApplication::applicationPid(), bytes.size(),
                                           QByteArray(64, '0')};
#if defined(Q_OS_WIN)
  CHECK(patchy::ui::launch_update_installer(launch).failure == patchy::ui::UpdateLaunchFailure::ChecksumMismatch);
  launch.installer_path = dir.filePath(QStringLiteral("absent.exe"));
  CHECK(patchy::ui::launch_update_installer(launch).failure == patchy::ui::UpdateLaunchFailure::InstallerMissing);
#else
  CHECK(!patchy::ui::launch_update_installer(launch).started());
#endif
#if defined(Q_OS_WIN)
  // Real signatures: Qt's DLL beside the suite is signed, the suite itself and the stand-in are not.
  const auto qt_signature = patchy::ui::file_signature(
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Qt6Core.dll")));
  CHECK(qt_signature.valid);
  CHECK(qt_signature.signer.contains(QStringLiteral("Qt Company")));
  CHECK(!patchy::ui::file_signature(QCoreApplication::applicationFilePath()).valid);
  CHECK(!patchy::ui::file_signature(path).valid);
#endif
}

// The whole one-click path with a file:// stand-in for the installer and the launch
// captured: a wrong checksum never reaches the installer, a cancelled save prompt or a
// launch that fails leaves Patchy open, and a settled close hands over exactly once.
void ui_update_one_click_downloads_installer_and_hands_over_on_close() {
#if defined(Q_OS_WIN)
  const auto root = patchy::ui::to_qstring(
      std::filesystem::absolute(patchy::test::unicode_artifact_dir(u8"one-click-update")));
  QDir(root).mkpath(QStringLiteral("source"));
  QByteArray installer_bytes;
  for (int i = 0; i < 180000; ++i) {
    installer_bytes.append(static_cast<char>(i % 241));
  }
  const auto source_path = QDir(root).filePath(QStringLiteral("source/PatchyWindowsInstaller.exe"));
  {
    QFile source(source_path);
    CHECK(source.open(QIODevice::WriteOnly) && source.write(installer_bytes) == installer_bytes.size());
  }
  const auto download_folder = QDir(root).filePath(QStringLiteral("download"));
  const auto downloaded_path = QDir(download_folder).filePath(QStringLiteral("PatchyWindowsInstaller.exe"));

  patchy::ui::UpdateInfo update{QStringLiteral("windows"), QStringLiteral("9.9"),
                                QUrl(QStringLiteral("https://rtsoft.com/patchy/PatchyWindowsInstaller.exe"))};
  update.installer_url = QUrl::fromLocalFile(source_path);
  update.installer_size = installer_bytes.size();
  update.installer_sha256 = QCryptographicHash::hash(installer_bytes, QCryptographicHash::Sha256).toHex();

  patchy::ui::MainWindow window;
  show_window(window);
  patchy::ui::MainWindowTestAccess::set_self_update_override(window, true);
  patchy::ui::MainWindowTestAccess::set_update_download_folder(window, download_folder);
  std::vector<patchy::ui::UpdateInstallerLaunch> launches;
  bool launcher_starts = true;
  patchy::ui::MainWindowTestAccess::set_update_installer_launcher(
      window, [&](const patchy::ui::UpdateInstallerLaunch& launch) {
        launches.push_back(launch);
        return launcher_starts
                   ? patchy::ui::UpdateLaunchResult{}
                   : patchy::ui::UpdateLaunchResult{patchy::ui::UpdateLaunchFailure::NotSigned,
                                                    QStringLiteral("Seth A. Robinson"), {}};
      });

  // Answers each dialog of one run as it appears.
  bool prompt_answered = false;
  bool saw_failure = false;
  bool saw_save_prompt = false;
  QString install_failure_text;
  auto save_answer = QMessageBox::Cancel;
  QTimer driver;
  QObject::connect(&driver, &QTimer::timeout, &window, [&] {
    if (auto* failure =
            qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateDownloadFailedMessageBox")))) {
      saw_failure = true;
      failure->reject();  // never the Download button: it opens a browser
      return;
    }
    if (auto* refused =
            qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateInstallFailedMessageBox")))) {
      install_failure_text = refused->text();
      refused->reject();  // never the Download button: it opens a browser
      return;
    }
    if (auto* save = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("saveChangesMessageBox")))) {
      saw_save_prompt = true;
      save->button(save_answer)->click();
      return;
    }
    if (prompt_answered) {
      return;
    }
    if (auto* prompt = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")))) {
      prompt_answered = true;
      auto* update_now = prompt->findChild<QAbstractButton*>(QStringLiteral("updateInstallNowButton"));
      CHECK(update_now != nullptr);
      if (update_now != nullptr) {
        update_now->click();
      } else {
        prompt->reject();
      }
    }
  });
  const auto run_update = [&](const patchy::ui::UpdateInfo& info) {
    prompt_answered = false;
    saw_failure = false;
    saw_save_prompt = false;
    install_failure_text.clear();
    driver.start(10);
    window.show_update_available(info);
    process_events_for(200);  // a refused installer is reported once the close event is over
    driver.stop();
    CHECK(prompt_answered);
  };

  // A download that does not match the manifest's checksum: reported, nothing kept.
  auto wrong = update;
  wrong.installer_sha256 = QCryptographicHash::hash(QByteArray("something else"), QCryptographicHash::Sha256).toHex();
  run_update(wrong);
  CHECK(saw_failure);
  CHECK(launches.empty());
  CHECK(window.isVisible());
  CHECK(!QFileInfo::exists(downloaded_path));

  // Unsaved work and Cancel at the save prompt: Patchy stays open, nothing is launched.
  require_action(window, "layerNewAction")->trigger();
  QApplication::processEvents();
  CHECK(patchy::ui::MainWindowTestAccess::active_session_is_modified(window));
  save_answer = QMessageBox::Cancel;
  run_update(update);
  CHECK(!saw_failure);
  CHECK(saw_save_prompt);
  CHECK(launches.empty());
  CHECK(window.isVisible());
  CHECK(!patchy::ui::MainWindowTestAccess::update_install_pending(window));
  {
    QFile downloaded(downloaded_path);
    CHECK(downloaded.open(QIODevice::ReadOnly) && downloaded.readAll() == installer_bytes);
  }

  // The installer cannot be started: Patchy stays open instead of quitting un-updated.
  save_answer = QMessageBox::Discard;
  launcher_starts = false;
  run_update(update);
  CHECK(saw_save_prompt);
  CHECK(launches.size() == 1);
  CHECK(window.isVisible());
  CHECK(!patchy::ui::MainWindowTestAccess::update_install_pending(window));
  // And says why, not only that it failed.
  CHECK(install_failure_text.contains(QStringLiteral("The update was not installed.")));
  CHECK(install_failure_text.contains(QStringLiteral("is not signed by Seth A. Robinson")));

  // Settled: the installer gets this copy's folder and process id, and the window closes.
  launcher_starts = true;
  run_update(update);
  CHECK(launches.size() == 2);
  if (launches.size() == 2) {
    CHECK(QFileInfo(launches.back().installer_path) == QFileInfo(downloaded_path));
    CHECK(launches.back().install_dir == QCoreApplication::applicationDirPath());
    CHECK(launches.back().wait_process_id == QCoreApplication::applicationPid());
    CHECK(launches.back().expected_size == installer_bytes.size());
    CHECK(launches.back().expected_sha256 == update.installer_sha256);
  }
  CHECK(!window.isVisible());
#endif
}

// The AppImage half of a one-click update (docs/auto-update.md, "AppImage"): the download is
// checked once more and renamed over the running file, which only then holds the new bytes;
// a damaged or missing download leaves the running file alone.
void update_appimage_replacement_checks_and_renames() {
  QTemporaryDir dir;
  CHECK(dir.isValid());
  const auto target = dir.filePath(QStringLiteral("PatchyLinux.AppImage"));
  const QByteArray old_bytes(4000, 'o');
  const QByteArray new_bytes(6000, 'n');
  const auto write = [](const QString& path, const QByteArray& bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size());
  };
  const auto read = [](const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
  };
  write(target, old_bytes);
  CHECK(patchy::ui::appimage_can_be_replaced(target));
  CHECK(!patchy::ui::appimage_can_be_replaced(dir.filePath(QStringLiteral("absent.AppImage"))));
  CHECK(!patchy::ui::appimage_can_be_replaced(QString()));

  const auto name = patchy::ui::appimage_update_download_name(target, QStringLiteral("9.9"));
  CHECK(name == QStringLiteral(".PatchyLinux.AppImage.update-9.9"));
  CHECK(patchy::ui::appimage_update_download_name(target, QStringLiteral("../9.9/x")) ==
        QStringLiteral(".PatchyLinux.AppImage.update-..9.9"));
  const auto downloaded = dir.filePath(name);
  const auto sha = QCryptographicHash::hash(new_bytes, QCryptographicHash::Sha256).toHex();

  patchy::ui::UpdateInstallerLaunch launch{downloaded, {}, 0, new_bytes.size(), sha, {}, target};
  CHECK(patchy::ui::replace_appimage(launch).failure == patchy::ui::UpdateLaunchFailure::InstallerMissing);
  write(downloaded, new_bytes);
  auto wrong = launch;
  wrong.expected_sha256 = QByteArray(64, '0');
  CHECK(patchy::ui::replace_appimage(wrong).failure == patchy::ui::UpdateLaunchFailure::ChecksumMismatch);
  CHECK(read(target) == old_bytes);
  CHECK(QFileInfo::exists(downloaded));

  CHECK(patchy::ui::replace_appimage(launch).started());
  CHECK(read(target) == new_bytes);
  CHECK(!QFileInfo::exists(downloaded));
#ifdef Q_OS_LINUX
  CHECK(QFileInfo(target).isExecutable());
#endif

  // A leftover download (an update cancelled at the save prompt) goes once that version runs;
  // nothing else in the folder is touched.
  write(downloaded, new_bytes);
  const auto unrelated = dir.filePath(QStringLiteral(".PatchyLinux.AppImage.update-9.8"));
  write(unrelated, old_bytes);
  patchy::ui::remove_appimage_update_download(target, QStringLiteral("9.9"));
  CHECK(!QFileInfo::exists(downloaded));
  CHECK(QFileInfo::exists(unrelated));
  CHECK(QFileInfo::exists(target));
}

// The whole AppImage one-click path on Linux, from the prompt: Update Now downloads the
// pinned AppImage beside the running file, the save prompt can still keep Patchy open, and a
// settled close hands the replacement over (captured here, then applied for real).
void ui_update_one_click_replaces_appimage_on_close() {
#if defined(Q_OS_LINUX)
  QTemporaryDir apps;
  CHECK(apps.isValid());
  const auto appimage = apps.filePath(QStringLiteral("PatchyLinux.AppImage"));
  {
    QFile running(appimage);
    CHECK(running.open(QIODevice::WriteOnly) && running.write(QByteArray(3000, 'o')) == 3000);
  }
  QByteArray new_bytes;
  for (int i = 0; i < 150000; ++i) {
    new_bytes.append(static_cast<char>(i % 239));
  }
  QTemporaryDir source_dir;
  CHECK(source_dir.isValid());
  const auto source_path = source_dir.filePath(QStringLiteral("PatchyLinux.AppImage"));
  {
    QFile source(source_path);
    CHECK(source.open(QIODevice::WriteOnly) && source.write(new_bytes) == new_bytes.size());
  }
  const AppImageEnvironment env(appimage.toUtf8(), appdir_containing_this_binary());
  CHECK(patchy::ui::appimage_file_path() == appimage);

  patchy::ui::UpdateInfo update{QStringLiteral("linux"), QStringLiteral("9.9"),
                                QUrl(QStringLiteral("https://rtsoft.com/patchy/PatchyLinux.flatpak"))};
  update.appimage_install_url = QUrl::fromLocalFile(source_path);
  update.appimage_install_size = new_bytes.size();
  update.appimage_install_sha256 = QCryptographicHash::hash(new_bytes, QCryptographicHash::Sha256).toHex();
  const auto downloaded = apps.filePath(QStringLiteral(".PatchyLinux.AppImage.update-9.9"));

  patchy::ui::MainWindow window;
  show_window(window);
  std::vector<patchy::ui::UpdateInstallerLaunch> launches;
  patchy::ui::MainWindowTestAccess::set_update_installer_launcher(
      window, [&](const patchy::ui::UpdateInstallerLaunch& launch) {
        launches.push_back(launch);
        return patchy::ui::UpdateLaunchResult{};
      });

  bool prompt_answered = false;
  bool saw_update_now = false;
  auto save_answer = QMessageBox::Cancel;
  QTimer driver;
  QObject::connect(&driver, &QTimer::timeout, &window, [&] {
    if (auto* save = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("saveChangesMessageBox")))) {
      save->button(save_answer)->click();
      return;
    }
    if (prompt_answered) {
      return;
    }
    if (auto* prompt = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")))) {
      prompt_answered = true;
      CHECK(prompt->text().contains(QStringLiteral("replace this AppImage")));
      CHECK(prompt->findChild<QAbstractButton*>(QStringLiteral("updateCopyCommandButton")) == nullptr);
      auto* update_now = prompt->findChild<QAbstractButton*>(QStringLiteral("updateInstallNowButton"));
      saw_update_now = update_now != nullptr;
      if (update_now != nullptr) {
        update_now->click();
      } else {
        prompt->reject();
      }
    }
  });
  const auto run_update = [&] {
    prompt_answered = false;
    driver.start(10);
    window.show_update_available(update);
    process_events_for(200);
    driver.stop();
    CHECK(prompt_answered);
  };

  // Unsaved work and Cancel at the save prompt: Patchy stays open, the download is kept.
  require_action(window, "layerNewAction")->trigger();
  QApplication::processEvents();
  save_answer = QMessageBox::Cancel;
  run_update();
  CHECK(saw_update_now);
  CHECK(launches.empty());
  CHECK(window.isVisible());
  CHECK(QFileInfo(downloaded).size() == new_bytes.size());

  // Settled: the replacement is handed over for this file, and the window closes.
  save_answer = QMessageBox::Discard;
  run_update();
  CHECK(launches.size() == 1);
  CHECK(!window.isVisible());
  if (launches.size() == 1) {
    const auto& launch = launches.front();
    CHECK(launch.appimage_target == appimage);
    CHECK(QFileInfo(launch.installer_path) == QFileInfo(downloaded));
    CHECK(launch.wait_process_id == QCoreApplication::applicationPid());
    CHECK(launch.expected_size == new_bytes.size());
    // The real replacement (launch_update_installer would also start the relaunch waiter).
    CHECK(patchy::ui::replace_appimage(launch).started());
    QFile replaced(appimage);
    CHECK(replaced.open(QIODevice::ReadOnly) && replaced.readAll() == new_bytes);
  }

  // A read-only folder cannot take the rename: the prompt falls back to the download.
  const AppImageEnvironment readonly_env("/usr/bin/PatchyLinux.AppImage", appdir_containing_this_binary());
  patchy::ui::MainWindow second;
  show_window(second);
  bool saw_download_prompt = false;
  QTimer::singleShot(0, [&] {
    auto* prompt = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")));
    CHECK(prompt != nullptr);
    if (prompt != nullptr) {
      saw_download_prompt =
          prompt->findChild<QAbstractButton*>(QStringLiteral("updateInstallNowButton")) == nullptr &&
          prompt->findChild<QAbstractButton*>(QStringLiteral("updateDownloadButton")) != nullptr;
      prompt->reject();
    }
  });
  second.show_update_available(update);
  CHECK(saw_download_prompt);
#endif
}

void ui_update_preference_persists_startup_check_setting() {
  SettingsValueRestorer restore_update_check(QStringLiteral("updates/checkOnStartup"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("updates/checkOnStartup"), false);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("updates/checkOnStartup"), true);
    settings.sync();
  }

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* check = dialog->findChild<QCheckBox*>(QStringLiteral("preferencesCheckForUpdatesCheck"));
    CHECK(check != nullptr);
    CHECK(check->isChecked());
    check->setChecked(false);
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);

  auto settings = patchy::ui::app_settings();
  CHECK(!settings.value(QStringLiteral("updates/checkOnStartup"), true).toBool());
}

// A store build (CMake PATCHY_STORE_BUILD) and PATCHY_NO_UPDATE_CHECK=1 share one switch:
// no startup request leaves the machine and Preferences offers no setting for it.
void ui_update_checks_can_be_switched_off() {
  CHECK(patchy::ui::update_checks_available());
  const EnvironmentVariableRestorer restore_switch("PATCHY_NO_UPDATE_CHECK");
  qputenv("PATCHY_NO_UPDATE_CHECK", "1");
  CHECK(!patchy::ui::update_checks_available());

  SettingsValueRestorer restore_update_check(QStringLiteral("updates/checkOnStartup"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("updates/checkOnStartup"), true);
    settings.sync();
  }
  ManifestServer server(R"({"platforms": {"windows": {"version": "9.9", "download_url": "https://rtsoft.com/w.exe"},
                                          "macos": {"version": "9.9", "download_url": "https://rtsoft.com/m.dmg"},
                                          "linux": {"version": "9.9", "download_url": "https://rtsoft.com/l.flatpak"}}})");
  const UpdateManifestUrlOverride override(server.manifest_url());

  patchy::ui::MainWindow window;
  show_window(window);
  window.begin_startup_update_check();
  process_events_for(500);
  CHECK(server.requests() == 0);
  CHECK(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")) == nullptr);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    CHECK(dialog->findChild<QCheckBox*>(QStringLiteral("preferencesCheckForUpdatesCheck")) == nullptr);
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);

  // Accepting Preferences without the checkbox leaves the stored choice alone.
  auto settings = patchy::ui::app_settings();
  CHECK(settings.value(QStringLiteral("updates/checkOnStartup"), false).toBool());
}

namespace {

// Opens Preferences, hands the dialog to `inspect`, then accepts or rejects it.
template <typename Inspect>
void run_preferences_dialog(patchy::ui::MainWindow& window, bool accept, Inspect inspect) {
  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    inspect(*dialog);
    saw_dialog = true;
    if (accept) {
      dialog->accept();
    } else {
      dialog->reject();
    }
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
}

// One wheel notch down, delivered as the window system delivers it. Qt hands only a
// spontaneous wheel on to the parent widgets (and gives WheelFocus widgets the focus first),
// and sendEvent clears that flag, so this goes through notify() the way QTest's key and
// mouse helpers do.
void send_wheel_notch_down(QWidget& target) {
  const QPointF center(target.width() / 2.0, target.height() / 2.0);
  QWheelEvent wheel(center, target.mapToGlobal(center), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                    Qt::NoScrollPhase, false);
  QSpontaneKeyEvent::setSpontaneous(&wheel);
  qApp->notify(&target, &wheel);
}

}  // namespace

// GitHub issue 92: a wheel that passes over a Preferences dropdown or number field scrolls
// the page and leaves the setting alone; only a control with keyboard focus takes the wheel.
void ui_preferences_wheel_over_unfocused_control_scrolls_the_page() {
  patchy::ui::MainWindow window;
  show_window(window);
  run_preferences_dialog(window, /*accept=*/false, [](QDialog& dialog) {
    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("preferencesTabWidget"));
    auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("preferencesGuiScaleCombo"));
    CHECK(tabs != nullptr && combo != nullptr);
    if (tabs == nullptr || combo == nullptr) {
      return;
    }
    dialog.resize(dialog.minimumSize());
    QApplication::processEvents();
    auto* scroll = tabs->currentWidget()->findChild<QScrollArea*>(QStringLiteral("preferencesTabScroll"));
    CHECK(scroll != nullptr);
    if (scroll == nullptr) {
      return;
    }
    auto* bar = scroll->verticalScrollBar();
    CHECK(bar->maximum() > 0);
    bar->setValue(0);

    // A real wheel focuses a WheelFocus widget before delivery, so the policy matters too.
    CHECK(combo->focusPolicy() == Qt::StrongFocus);
    combo->clearFocus();
    CHECK(combo->count() > 1);
    combo->setCurrentIndex(0);
    send_wheel_notch_down(*combo);
    CHECK(combo->currentIndex() == 0);
    CHECK(bar->value() > 0);

    // With keyboard focus the wheel is the control's again.
    combo->setFocus();
    QApplication::processEvents();
    CHECK(combo->hasFocus());
    // (macOS combo boxes never take the wheel; the style says so.)
    if (combo->style()->styleHint(QStyle::SH_ComboBox_AllowWheelScrolling, nullptr, combo) != 0) {
      const int scrolled = bar->value();
      send_wheel_notch_down(*combo);
      CHECK(combo->currentIndex() == 1);
      CHECK(bar->value() == scrolled);
    }
    combo->clearFocus();

    // Spin boxes on the other tabs are covered by the same guard.
    auto* spin = dialog.findChild<QAbstractSpinBox*>(QStringLiteral("preferencesGridSpacingSpin"));
    CHECK(spin != nullptr);
    if (spin != nullptr) {
      CHECK(spin->focusPolicy() == Qt::StrongFocus);
      const auto before = spin->text();
      send_wheel_notch_down(*spin);
      CHECK(spin->text() == before);
    }
  });
}

// GitHub issue 92: the frameless Preferences dialog resizes by its corner grip, its pages
// grow with it, and the size comes back the next time it opens.
void ui_preferences_dialog_resizes_and_remembers_its_size() {
  SettingsValueRestorer restore_size(QStringLiteral("preferences/dialogSize"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("preferences/dialogSize"));
    settings.sync();
  }
  patchy::ui::MainWindow window;
  show_window(window);

  QSize target;
  run_preferences_dialog(window, /*accept=*/false, [&](QDialog& dialog) {
    CHECK(dialog.findChild<QSizeGrip*>(QStringLiteral("preferencesSizeGrip")) != nullptr);
    auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("preferencesTabWidget"));
    CHECK(tabs != nullptr);
    if (tabs == nullptr) {
      return;
    }
    CHECK(dialog.maximumHeight() > dialog.minimumHeight());
    const auto entry_size = dialog.size();
    dialog.resize(dialog.minimumSize());
    QApplication::processEvents();
    const int small_tabs_height = tabs->height();
    // Taller only: the tab row can make the minimum width as wide as the offscreen screen,
    // and a restored size is kept inside the screen.
    target = dialog.minimumSize() + QSize(0, 90);
    CHECK(target != entry_size);
    dialog.resize(target);
    QApplication::processEvents();
    CHECK(dialog.size() == target);
    CHECK(tabs->height() > small_tabs_height);
  });
  CHECK(target.isValid());
  CHECK(patchy::ui::stored_preferences_dialog_size() == target);

  run_preferences_dialog(window, /*accept=*/false, [&](QDialog& dialog) { CHECK(dialog.size() == target); });
}

// GitHub issue 92: the interval gate of the startup update check, without any network.
void ui_update_check_interval_gates_the_startup_check() {
  using patchy::ui::startup_update_check_due;
  constexpr qint64 kDay = 24 * 60 * 60;
  constexpr qint64 kNow = 1'800'000'000;

  // Every launch is the default and always due.
  CHECK(patchy::ui::kDefaultUpdateCheckIntervalDays == 0);
  CHECK(startup_update_check_due(0, kNow - 1, kNow));
  CHECK(startup_update_check_due(0, kNow, kNow));
  for (const int days : {7, 14, 30}) {
    CHECK(!startup_update_check_due(days, kNow - 1, kNow));
    CHECK(!startup_update_check_due(days, kNow - days * kDay + 1, kNow));
    CHECK(startup_update_check_due(days, kNow - days * kDay, kNow));
    CHECK(startup_update_check_due(days, kNow - days * kDay - 1, kNow));
    // Never checked, or a record from the future (the clock moved back).
    CHECK(startup_update_check_due(days, 0, kNow));
    CHECK(startup_update_check_due(days, kNow + kDay, kNow));
  }

  for (const int days : patchy::ui::kUpdateCheckIntervalDays) {
    CHECK(patchy::ui::normalize_update_check_interval_days(days) == days);
  }
  CHECK(patchy::ui::normalize_update_check_interval_days(5) == 0);
  CHECK(patchy::ui::normalize_update_check_interval_days(-7) == 0);

  SettingsValueRestorer restore_interval(QStringLiteral("updates/checkIntervalDays"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("updates/checkIntervalDays"));
    settings.sync();
  }
  CHECK(patchy::ui::stored_update_check_interval_days() == 0);

  using patchy::ui::update_prompt_wanted;
  CHECK(update_prompt_wanted(QStringLiteral("9.9"), QString()));
  CHECK(update_prompt_wanted(QStringLiteral("9.9"), QStringLiteral("not a version")));
  CHECK(!update_prompt_wanted(QStringLiteral("9.9"), QStringLiteral("9.9")));
  CHECK(!update_prompt_wanted(QStringLiteral("9.8"), QStringLiteral("9.9")));
  CHECK(update_prompt_wanted(QStringLiteral("9.10"), QStringLiteral("9.9")));
}

// The startup check against a loopback manifest: skipped inside the interval, run and
// recorded once it has passed, and always run on "every launch".
void ui_update_startup_check_honors_stored_interval() {
  SettingsValueRestorer restore_update_check(QStringLiteral("updates/checkOnStartup"));
  SettingsValueRestorer restore_interval(QStringLiteral("updates/checkIntervalDays"));
  SettingsValueRestorer restore_last_check(QStringLiteral("updates/lastCheckSecsSinceEpoch"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("updates/checkOnStartup"), true);
    settings.sync();
  }
  // Nothing newer, so no prompt opens.
  ManifestServer server(R"({"platforms": {"windows": {"version": "0.0.1", "download_url": "https://rtsoft.com/w.exe"},
                                          "macos": {"version": "0.0.1", "download_url": "https://rtsoft.com/m.dmg"},
                                          "linux": {"version": "0.0.1", "download_url": "https://rtsoft.com/l.flatpak"}}})");
  const UpdateManifestUrlOverride override(server.manifest_url());
  patchy::ui::MainWindow window;
  show_window(window);

  constexpr qint64 kDay = 24 * 60 * 60;
  const auto now = QDateTime::currentSecsSinceEpoch();

  patchy::ui::set_stored_update_check_interval_days(7);
  patchy::ui::set_stored_update_last_check_secs(now - 3600);
  window.begin_startup_update_check();
  process_events_for(500);
  CHECK(server.requests() == 0);
  CHECK(patchy::ui::stored_update_last_check_secs() == now - 3600);

  patchy::ui::set_stored_update_last_check_secs(now - 8 * kDay);
  window.begin_startup_update_check();
  CHECK(process_events_until([&] { return patchy::ui::stored_update_last_check_secs() >= now; }, 15000));
  CHECK(server.requests() == 1);

  // The record is fresh now, but "every launch" checks regardless.
  patchy::ui::set_stored_update_check_interval_days(0);
  window.begin_startup_update_check();
  CHECK(process_events_until([&] { return server.requests() == 2; }, 15000));
}

void ui_update_preference_persists_check_interval() {
  SettingsValueRestorer restore_update_check(QStringLiteral("updates/checkOnStartup"));
  SettingsValueRestorer restore_interval(QStringLiteral("updates/checkIntervalDays"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("updates/checkOnStartup"), true);
    settings.remove(QStringLiteral("updates/checkIntervalDays"));
    settings.sync();
  }
  patchy::ui::MainWindow window;
  show_window(window);

  run_preferences_dialog(window, /*accept=*/true, [](QDialog& dialog) {
    auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("preferencesUpdateCheckIntervalCombo"));
    CHECK(combo != nullptr);
    if (combo == nullptr) {
      return;
    }
    // Every launch stays the default; the stored value is the day count, never an index.
    CHECK(combo->isEnabled());
    CHECK(combo->currentData().toInt() == 0);
    CHECK(combo->currentText() == QStringLiteral("Every launch"));
    QList<int> days;
    for (int index = 0; index < combo->count(); ++index) {
      days.push_back(combo->itemData(index).toInt());
    }
    CHECK(days == QList<int>({0, 7, 14, 30}));
    combo->setCurrentIndex(combo->findData(14));
  });
  CHECK(patchy::ui::stored_update_check_interval_days() == 14);
  CHECK(patchy::ui::app_settings().value(QStringLiteral("updates/checkIntervalDays")).toInt() == 14);

  run_preferences_dialog(window, /*accept=*/false, [](QDialog& dialog) {
    auto* combo = dialog.findChild<QComboBox*>(QStringLiteral("preferencesUpdateCheckIntervalCombo"));
    auto* check = dialog.findChild<QCheckBox*>(QStringLiteral("preferencesCheckForUpdatesCheck"));
    CHECK(combo != nullptr && check != nullptr);
    if (combo == nullptr || check == nullptr) {
      return;
    }
    CHECK(combo->currentData().toInt() == 14);
    check->setChecked(false);
    CHECK(!combo->isEnabled());
  });
}

// "Don't ask again about version X" on the update prompt silences the startup prompt for
// that version only: the start panel still reports it, and a later release asks again.
void ui_update_prompt_skip_version_silences_until_a_newer_one() {
  SettingsValueRestorer restore_update_check(QStringLiteral("updates/checkOnStartup"));
  SettingsValueRestorer restore_interval(QStringLiteral("updates/checkIntervalDays"));
  SettingsValueRestorer restore_last_check(QStringLiteral("updates/lastCheckSecsSinceEpoch"));
  SettingsValueRestorer restore_skipped(QStringLiteral("updates/skippedVersion"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("updates/checkOnStartup"), true);
    settings.setValue(QStringLiteral("updates/checkIntervalDays"), 0);
    settings.remove(QStringLiteral("updates/skippedVersion"));
    settings.sync();
  }
  patchy::ui::MainWindow window;
  show_window(window);

  // Closes every update prompt that opens, ticking the checkbox when asked to.
  int prompts = 0;
  bool tick_skip = true;
  QTimer closer;
  QObject::connect(&closer, &QTimer::timeout, &closer, [&] {
    auto* dialog = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("updateAvailableMessageBox")));
    if (dialog == nullptr) {
      return;
    }
    ++prompts;
    auto* skip = dialog->findChild<QCheckBox*>(QStringLiteral("updateSkipVersionCheck"));
    CHECK(skip != nullptr);
    if (skip != nullptr) {
      CHECK(skip->text() == QStringLiteral("Don't ask again about version %1").arg(prompts == 1 ? QStringLiteral("9.9") : QStringLiteral("9.10")));
      CHECK(!skip->isChecked());
      skip->setChecked(tick_skip);
    }
    dialog->reject();
  });
  closer.start(20);

  window.show_update_available(
      {QStringLiteral("windows"), QStringLiteral("9.9"), QUrl(QStringLiteral("https://rtsoft.com/w.exe"))});
  CHECK(prompts == 1);
  CHECK(patchy::ui::stored_update_skipped_version() == QStringLiteral("9.9"));

  const auto manifest = [](const char* version) {
    return QByteArray(R"({"platforms": {"windows": {"version": "V", "download_url": "https://rtsoft.com/w.exe"},
                                         "macos": {"version": "V", "download_url": "https://rtsoft.com/m.dmg"},
                                         "linux": {"version": "V", "download_url": "https://rtsoft.com/l.flatpak"}}})")
        .replace("V", version);
  };
  {
    ManifestServer server(manifest("9.9"));
    const UpdateManifestUrlOverride override(server.manifest_url());
    window.begin_startup_update_check();
    CHECK(process_events_until([&] { return server.requests() == 1; }, 15000));
    process_events_for(300);
    CHECK(prompts == 1);
    auto* status = window.findChild<QLabel*>(QStringLiteral("startPanelUpdateStatus"));
    CHECK(status != nullptr);
    if (status != nullptr) {
      CHECK(status->text().contains(QStringLiteral("9.9")));
    }
  }
  {
    tick_skip = false;
    ManifestServer server(manifest("9.10"));
    const UpdateManifestUrlOverride override(server.manifest_url());
    window.begin_startup_update_check();
    CHECK(process_events_until([&] { return prompts == 2; }, 15000));
    // Not ticked this time: the stored choice is left alone.
    CHECK(patchy::ui::stored_update_skipped_version() == QStringLiteral("9.9"));
  }
}

struct GuiScaleDialogRun {
  bool saw_dialog{false};
  bool dismissed_message{false};
  int entry_percent{0};
};

// Opens Preferences, records what the interface-scale combo shows on entry, optionally
// selects `percent`, and accepts. A changed scale raises a modal restart/reload notice
// that has to be dismissed before the dialog can finish closing.
GuiScaleDialogRun drive_preferences_gui_scale(patchy::ui::MainWindow& window,
                                              std::optional<int> percent) {
  GuiScaleDialogRun run;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesGuiScaleCombo"));
    CHECK(combo != nullptr);
    if (combo == nullptr) {
      dialog->reject();
      return;
    }
    run.entry_percent = combo->currentData().toInt();
    run.saw_dialog = true;
    if (percent.has_value()) {
      const int index = combo->findData(*percent);
      CHECK(index >= 0);
      combo->setCurrentIndex(index);
      QTimer::singleShot(0, [&] {
        auto* message = qobject_cast<QMessageBox*>(
            find_top_level_dialog(QStringLiteral("preferencesInterfaceScaleMessageBox")));
        CHECK(message != nullptr);
        if (message != nullptr) {
          message->accept();
          run.dismissed_message = true;
        }
      });
    }
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  return run;
}

void ui_gui_scale_preference_persists_setting() {
  SettingsValueRestorer restore_gui_scale(QStringLiteral("preferences/guiScalePercent"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/guiScalePercent"), 100);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  const auto run = drive_preferences_gui_scale(window, 150);
  CHECK(run.saw_dialog);
  CHECK(run.dismissed_message);
  CHECK(run.entry_percent == 100);
  CHECK(patchy::ui::stored_gui_scale_percent() == 150);
}

// The steps below 100% are what let a browser tab match the desktop build's apparent
// size, so keep one of them exercised end to end through the combo.
void ui_gui_scale_preference_persists_step_below_full_size() {
  SettingsValueRestorer restore_gui_scale(QStringLiteral("preferences/guiScalePercent"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/guiScalePercent"), 100);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  const auto run = drive_preferences_gui_scale(window, 75);
  CHECK(run.saw_dialog);
  CHECK(run.dismissed_message);
  CHECK(patchy::ui::stored_gui_scale_percent() == 75);
}

// A stored value that is not one of the offered steps (a hand-edited ini, a step a later
// build dropped) must never reach QT_SCALE_FACTOR. The combo shows the platform default
// instead, and accepting without touching it writes nothing and raises no notice.
void ui_gui_scale_preference_falls_back_for_unknown_step() {
  SettingsValueRestorer restore_gui_scale(QStringLiteral("preferences/guiScalePercent"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/guiScalePercent"), 110);
    settings.sync();
  }
  CHECK(patchy::ui::stored_gui_scale_percent() == patchy::ui::kDefaultGuiScalePercent);

  patchy::ui::MainWindow window;
  show_window(window);

  const auto run = drive_preferences_gui_scale(window, std::nullopt);
  CHECK(run.saw_dialog);
  CHECK(!run.dismissed_message);
  CHECK(run.entry_percent == patchy::ui::kDefaultGuiScalePercent);
}

void gui_scale_percent_normalizes_to_offered_steps() {
  for (const int percent : patchy::ui::kGuiScalePercents) {
    CHECK(patchy::ui::normalize_gui_scale_percent(percent) == percent);
  }
  // Anything off the list, in either direction, lands on the platform default.
  for (const int percent : {-50, 0, 66, 110, 201, 400}) {
    CHECK(patchy::ui::normalize_gui_scale_percent(percent) ==
          patchy::ui::kDefaultGuiScalePercent);
  }
  // The desktop build must start at full size; only the web build shrinks by default.
  CHECK(patchy::ui::kDefaultGuiScalePercent == 100);
}

// Drives the Preferences color-scheme combo and returns the dialog result, so the
// persist and cancel tests differ only in how they leave the dialog.
int choose_preferences_color_scheme(patchy::ui::MainWindow& window, const QString& token,
                                    bool accept) {
  bool saw_dialog = false;
  int result = QDialog::Rejected;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesColorSchemeCombo"));
    CHECK(combo != nullptr);
    if (combo == nullptr) {
      dialog->reject();
      return;
    }
    const auto index = combo->findData(token);
    CHECK(index >= 0);
    combo->setCurrentIndex(index);
    saw_dialog = true;
    result = accept ? QDialog::Accepted : QDialog::Rejected;
    if (accept) {
      dialog->accept();
    } else {
      dialog->reject();
    }
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
  return result;
}

void ui_color_scheme_preference_persists_setting() {
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  ColorSchemeRestorer restore_active;

  patchy::ui::MainWindow window;
  show_window(window);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Dark);

  choose_preferences_color_scheme(window, QStringLiteral("light"), /*accept=*/true);

  auto settings = patchy::ui::app_settings();
  CHECK(settings.value(QStringLiteral("preferences/colorScheme")).toString() ==
        QStringLiteral("light"));
  CHECK(patchy::ui::ThemeManager::instance().preference() == patchy::ui::ColorSchemePreference::Light);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Light);

  // Unlike interface scale, the scheme applies immediately, so no restart notice
  // may appear. A stray modal here would also hang the rest of the suite.
  CHECK(find_top_level_dialog(QStringLiteral("preferencesInterfaceScaleMessageBox")) == nullptr);
}

void ui_color_scheme_cancel_restores_entry_scheme() {
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  // Seeded rather than asserted absent: accepting Preferences persists the scheme
  // like every other setting on that page, so any earlier test that accepted the
  // dialog has already written this key.
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/colorScheme"), QStringLiteral("dark"));
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  // The combo previews live; closing without accepting has to undo the preview.
  choose_preferences_color_scheme(window, QStringLiteral("light"), /*accept=*/false);
  QApplication::processEvents();

  CHECK(patchy::ui::ThemeManager::instance().preference() == patchy::ui::ColorSchemePreference::Dark);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Dark);
  auto settings = patchy::ui::app_settings();
  CHECK(settings.value(QStringLiteral("preferences/colorScheme")).toString() == QStringLiteral("dark"));
}

void ui_color_scheme_follow_system_tracks_style_hints() {
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  ColorSchemeRestorer restore_active;

  auto& manager = patchy::ui::ThemeManager::instance();
  manager.set_preference(patchy::ui::ColorSchemePreference::FollowSystem, /*persist=*/false);

  // The offscreen platform reports Qt::ColorScheme::Unknown and never emits
  // colorSchemeChanged, so the system side is driven through the test seam.
  manager.set_system_color_scheme_for_testing(Qt::ColorScheme::Light);
  CHECK(manager.resolved_scheme() == patchy::ui::ColorScheme::Light);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Light);

  manager.set_system_color_scheme_for_testing(Qt::ColorScheme::Dark);
  CHECK(manager.resolved_scheme() == patchy::ui::ColorScheme::Dark);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Dark);

  // Unknown is what every platform without a color-scheme notion reports; it must
  // land on Patchy's historical Dark rather than an invalid state.
  manager.set_system_color_scheme_for_testing(Qt::ColorScheme::Unknown);
  CHECK(manager.resolved_scheme() == patchy::ui::ColorScheme::Dark);

  // An explicit choice ignores the system entirely.
  manager.set_preference(patchy::ui::ColorSchemePreference::Light, /*persist=*/false);
  manager.set_system_color_scheme_for_testing(Qt::ColorScheme::Dark);
  CHECK(manager.resolved_scheme() == patchy::ui::ColorScheme::Light);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Light);
}

// Exports the full palette, tweaks are not needed to prove fidelity: two
// deliberately-changed roles (one carrying alpha, to prove #RRGGBBAA
// round-trips) are enough, but every role is compared so a role the writer or
// the reader table silently skipped would still show up here.
void ui_theme_file_round_trips_full_palette() {
  auto palette = patchy::ui::dark_palette();
  palette.accent = QColor(0x12, 0x34, 0x56);
  palette.window_bg = QColor(0xAA, 0xBB, 0xCC, 0x80);

  const auto json =
      patchy::ui::serialize_theme_to_json(palette, patchy::ui::ColorScheme::Dark, QStringLiteral("Round Trip"));
  const auto result = patchy::ui::load_theme_from_json(json);

  CHECK(result.error.isEmpty());
  CHECK(result.warnings.isEmpty());
  CHECK(result.theme.has_value());
  CHECK(result.theme->name == QStringLiteral("Round Trip"));
  CHECK(result.theme->base == patchy::ui::ColorScheme::Dark);

  for (const auto& [name, member] : patchy::ui::theme_palette_roles()) {
    const auto original = palette.*member;
    const auto loaded = result.theme->palette.*member;
    if (original.rgba() != loaded.rgba()) {
      fprintf(stderr, "  role \"%s\" did not round-trip\n", QString(name).toUtf8().constData());
    }
    CHECK(original.rgba() == loaded.rgba());
  }
}

// A role the file never mentions keeps the declared base scheme's built-in
// value; only roles actually present in "roles" are overridden.
void ui_theme_file_missing_role_falls_back_to_base() {
  QJsonObject roles;
  roles.insert(QStringLiteral("accent"), QStringLiteral("#123456"));
  QJsonObject object;
  object.insert(QStringLiteral("base"), QStringLiteral("dark"));
  object.insert(QStringLiteral("roles"), roles);
  const auto json = QJsonDocument(object).toJson();

  const auto result = patchy::ui::load_theme_from_json(json);
  CHECK(result.error.isEmpty());
  CHECK(result.theme.has_value());
  CHECK(result.theme->palette.accent == QColor(0x12, 0x34, 0x56));
  CHECK(result.theme->palette.window_bg == patchy::ui::dark_palette().window_bg);
}

// A malformed color is a hard load error naming the offending role, not a
// warning: nothing downstream should ever see a half-loaded custom theme.
void ui_theme_file_invalid_hex_is_a_hard_error() {
  QJsonObject roles;
  roles.insert(QStringLiteral("accent"), QStringLiteral("not-a-color"));
  QJsonObject object;
  object.insert(QStringLiteral("base"), QStringLiteral("dark"));
  object.insert(QStringLiteral("roles"), roles);
  const auto json = QJsonDocument(object).toJson();

  const auto result = patchy::ui::load_theme_from_json(json);
  CHECK(!result.theme.has_value());
  CHECK(!result.error.isEmpty());
  CHECK(result.error.contains(QStringLiteral("accent")));
  CHECK(result.warnings.isEmpty());
}

// An unknown role name is forward-compatibility tolerance, not a failure: a
// theme authored against a newer build (with roles this build never heard of)
// still has to load on this one.
void ui_theme_file_unknown_role_is_a_warning_not_an_error() {
  QJsonObject roles;
  roles.insert(QStringLiteral("accent"), QStringLiteral("#123456"));
  roles.insert(QStringLiteral("totally_not_a_role"), QStringLiteral("#ffffff"));
  QJsonObject object;
  object.insert(QStringLiteral("base"), QStringLiteral("dark"));
  object.insert(QStringLiteral("roles"), roles);
  const auto json = QJsonDocument(object).toJson();

  const auto result = patchy::ui::load_theme_from_json(json);
  CHECK(result.error.isEmpty());
  CHECK(result.theme.has_value());
  CHECK(result.theme->palette.accent == QColor(0x12, 0x34, 0x56));
  CHECK(!result.warnings.isEmpty());
  CHECK(result.warnings.join(QLatin1Char('\n')).contains(QStringLiteral("totally_not_a_role")));
}

// "base" is the one field with no fallback: anything other than the two
// tokens ThemeManager already persists is a hard error, never a guess.
void ui_theme_file_invalid_base_is_a_hard_error() {
  QJsonObject object;
  object.insert(QStringLiteral("base"), QStringLiteral("purple"));
  const auto json = QJsonDocument(object).toJson();

  const auto result = patchy::ui::load_theme_from_json(json);
  CHECK(!result.theme.has_value());
  CHECK(!result.error.isEmpty());
}

// set_custom_theme never routes through apply_resolved_scheme(), whose
// equal-scheme guard would otherwise swallow a custom theme that declares the
// same base as the scheme already active. Assert the generation still bumps,
// theme() reflects the new colors, and the photoshop_style() cache (keyed on
// theme_generation(), not on light/dark) picks the change up.
void ui_custom_theme_applies_over_matching_base_and_bumps_generation() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);

  const auto generation_before = patchy::ui::theme_generation();
  const auto style_before = patchy::ui::photoshop_style();

  patchy::ui::CustomTheme custom;
  custom.name = QStringLiteral("Distinctive");
  custom.base = patchy::ui::ColorScheme::Dark;
  custom.palette = patchy::ui::dark_palette();
  custom.palette.window_bg = QColor(1, 2, 3);

  patchy::ui::ThemeManager::instance().set_custom_theme(QStringLiteral("distinctive.patchytheme"), custom,
                                                         /*persist=*/false);

  CHECK(patchy::ui::theme_generation() > generation_before);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Dark);
  CHECK(patchy::ui::theme().window_bg == QColor(1, 2, 3));
  CHECK(patchy::ui::has_active_custom_palette());

  const auto style_after = patchy::ui::photoshop_style();
  CHECK(!style_after.isEmpty());
  CHECK(!style_after.contains(QLatin1Char('@')));
  CHECK(style_after != style_before);
}

// A built-in and a custom theme are mutually exclusive: choosing a built-in
// scheme always wins, even while a custom theme is active.
void ui_custom_theme_cleared_by_switching_to_a_builtin_scheme() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);

  patchy::ui::CustomTheme custom;
  custom.base = patchy::ui::ColorScheme::Dark;
  custom.palette = patchy::ui::dark_palette();
  patchy::ui::ThemeManager::instance().set_custom_theme(QStringLiteral("temp.patchytheme"), custom,
                                                         /*persist=*/false);
  CHECK(patchy::ui::ThemeManager::instance().active_custom_theme_id().has_value());

  patchy::ui::ThemeManager::instance().set_preference(patchy::ui::ColorSchemePreference::Light,
                                                       /*persist=*/false);

  CHECK(!patchy::ui::ThemeManager::instance().active_custom_theme_id().has_value());
  CHECK(!patchy::ui::has_active_custom_palette());
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Light);
}

// Redirects user_themes_directory() to a scratch folder for one test, so
// import/export/reload tests never touch the real AppData themes folder.
class ThemesDirEnvGuard {
public:
  ThemesDirEnvGuard() : previous_(qgetenv("PATCHY_THEMES_DIR")), had_previous_(qEnvironmentVariableIsSet("PATCHY_THEMES_DIR")) {
    CHECK(dir_.isValid());
    qputenv("PATCHY_THEMES_DIR", dir_.path().toUtf8());
  }
  ~ThemesDirEnvGuard() {
    if (had_previous_) {
      qputenv("PATCHY_THEMES_DIR", previous_);
    } else {
      qunsetenv("PATCHY_THEMES_DIR");
    }
  }
  [[nodiscard]] QString path() const { return dir_.path(); }

private:
  QTemporaryDir dir_;
  QByteArray previous_;
  bool had_previous_;
};

// Mirrors what ThemeManager::load_saved_preference() does at startup: read
// the persisted preference, then read and apply a persisted custom theme id.
// Calling it a second time on the live singleton is the established pattern
// for "simulated restart" in this suite (see the language-preference tests).
void ui_custom_theme_id_persists_and_reapplies_like_a_restart() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  SettingsValueRestorer restore_custom_id(QStringLiteral("preferences/customThemeId"));
  ThemesDirEnvGuard themes_dir;

  auto palette = patchy::ui::dark_palette();
  palette.window_bg = QColor(9, 8, 7);
  const auto json = patchy::ui::serialize_theme_to_json(palette, patchy::ui::ColorScheme::Dark,
                                                          QStringLiteral("Restart Test"));
  QFile file(QDir(themes_dir.path()).filePath(QStringLiteral("restart-test.patchytheme")));
  CHECK(file.open(QIODevice::WriteOnly));
  file.write(json);
  file.close();

  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/colorScheme"), QStringLiteral("dark"));
    settings.setValue(QStringLiteral("preferences/customThemeId"), QStringLiteral("restart-test.patchytheme"));
    settings.sync();
  }

  patchy::ui::ThemeManager::instance().load_saved_preference();

  const auto active_id = patchy::ui::ThemeManager::instance().active_custom_theme_id();
  CHECK(active_id.has_value());
  CHECK(*active_id == QStringLiteral("restart-test.patchytheme"));
  CHECK(patchy::ui::theme().window_bg == QColor(9, 8, 7));
}

// A custom theme id whose backing file has been moved or deleted outside
// Patchy must not fail startup: fall back cleanly to the built-in preference
// already applied.
void ui_custom_theme_missing_file_falls_back_to_builtin_preference() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  SettingsValueRestorer restore_custom_id(QStringLiteral("preferences/customThemeId"));
  ThemesDirEnvGuard themes_dir;

  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/colorScheme"), QStringLiteral("light"));
    settings.setValue(QStringLiteral("preferences/customThemeId"), QStringLiteral("does-not-exist.patchytheme"));
    settings.sync();
  }

  patchy::ui::ThemeManager::instance().load_saved_preference();

  CHECK(!patchy::ui::ThemeManager::instance().active_custom_theme_id().has_value());
  CHECK(!patchy::ui::has_active_custom_palette());
  CHECK(patchy::ui::ThemeManager::instance().preference() == patchy::ui::ColorSchemePreference::Light);
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Light);
}

// A custom palette must resolve exactly like a built-in one: no leftover
// @token in the resolved stylesheet or in an ad hoc apply_theme_tokens() call.
void ui_custom_theme_qss_resolves_every_token() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);

  patchy::ui::CustomTheme custom;
  custom.base = patchy::ui::ColorScheme::Dark;
  custom.palette = patchy::ui::dark_palette();
  custom.palette.accent = QColor(0x77, 0x22, 0x99);
  patchy::ui::ThemeManager::instance().set_custom_theme(QStringLiteral("qss-check.patchytheme"), custom,
                                                         /*persist=*/false);

  const auto style = patchy::ui::photoshop_style();
  CHECK(!style.isEmpty());
  CHECK(!style.contains(QLatin1Char('@')));

  const auto resolved = patchy::ui::apply_theme_tokens(QStringLiteral("a: @accent;"));
  CHECK(resolved == QStringLiteral("a: %1;").arg(custom.palette.accent.name(QColor::HexRgb)));
}

// "format" is the one key that can refuse a file outright: a newer number means
// keys this build cannot interpret, and a silent partial load would be worse
// than an error. An absent key is format 1, so files written before the key
// existed still load.
void ui_theme_file_newer_format_is_a_hard_error() {
  QJsonObject object;
  object.insert(QStringLiteral("format"), 2);
  object.insert(QStringLiteral("base"), QStringLiteral("dark"));
  const auto newer = patchy::ui::load_theme_from_json(QJsonDocument(object).toJson());
  CHECK(!newer.theme.has_value());
  CHECK(newer.error.contains(QStringLiteral("2")));

  object.insert(QStringLiteral("format"), QStringLiteral("1"));
  const auto text = patchy::ui::load_theme_from_json(QJsonDocument(object).toJson());
  CHECK(!text.theme.has_value());

  object.remove(QStringLiteral("format"));
  const auto absent = patchy::ui::load_theme_from_json(QJsonDocument(object).toJson());
  CHECK(absent.theme.has_value());

  const auto exported = patchy::ui::serialize_theme_to_json(patchy::ui::dark_palette(), patchy::ui::ColorScheme::Dark,
                                                            QStringLiteral("Format"));
  CHECK(QJsonDocument::fromJson(exported).object().value(QStringLiteral("format")).toInt() ==
        patchy::ui::kThemeFileFormat);
}

void write_theme_file(const QString& path, const QColor& window_bg, const QString& name) {
  auto palette = patchy::ui::dark_palette();
  palette.window_bg = window_bg;
  QFile file(path);
  CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(patchy::ui::serialize_theme_to_json(palette, patchy::ui::ColorScheme::Dark, name));
  file.close();
}

// The authoring loop: edit the file in a text editor, click Reload Themes. The
// combo keeps the same entry selected and the fresh colors apply even though
// the selection never moved.
void ui_preferences_reload_reapplies_an_edited_theme_file() {
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  SettingsValueRestorer restore_custom_id(QStringLiteral("preferences/customThemeId"));
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  ThemesDirEnvGuard themes_dir;
  const auto path = QDir(themes_dir.path()).filePath(QStringLiteral("editable.patchytheme"));
  write_theme_file(path, QColor(10, 20, 30), QStringLiteral("Editable"));

  patchy::ui::MainWindow window;
  show_window(window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesColorSchemeCombo"));
    auto* reload = dialog->findChild<QPushButton*>(QStringLiteral("preferencesReloadThemesButton"));
    auto* remove = dialog->findChild<QPushButton*>(QStringLiteral("preferencesDeleteThemeButton"));
    CHECK(combo != nullptr && reload != nullptr && remove != nullptr);
    if (combo == nullptr || reload == nullptr || remove == nullptr) {
      dialog->reject();
      return;
    }
    CHECK(!remove->isEnabled());
    const auto index = combo->findData(QStringLiteral("custom:editable.patchytheme"));
    CHECK(index >= 0);
    CHECK(combo->itemText(index) == QStringLiteral("Editable"));
    combo->setCurrentIndex(index);
    QApplication::processEvents();
    CHECK(patchy::ui::theme().window_bg == QColor(10, 20, 30));
    CHECK(remove->isEnabled());

    write_theme_file(path, QColor(40, 50, 60), QStringLiteral("Editable"));
    reload->click();
    QApplication::processEvents();
    CHECK(combo->currentData().toString() == QStringLiteral("custom:editable.patchytheme"));
    CHECK(patchy::ui::theme().window_bg == QColor(40, 50, 60));
    CHECK(remove->isEnabled());
    saw_dialog = true;
    dialog->reject();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);

  // Rejecting restores the entry scheme, which was the built-in Dark.
  CHECK(!patchy::ui::has_active_custom_palette());
  CHECK(patchy::ui::active_color_scheme() == patchy::ui::ColorScheme::Dark);
}

// Delete removes the file and its entry, and the dialog falls back to the
// first built-in entry so the live preview and the revert guard stay in step.
void ui_preferences_delete_removes_theme_file_and_entry() {
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  SettingsValueRestorer restore_custom_id(QStringLiteral("preferences/customThemeId"));
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  ThemesDirEnvGuard themes_dir;
  const auto path = QDir(themes_dir.path()).filePath(QStringLiteral("doomed.patchytheme"));
  write_theme_file(path, QColor(70, 80, 90), QStringLiteral("Doomed"));

  patchy::ui::MainWindow window;
  show_window(window);

  bool saw_dialog = false;
  bool saw_confirm = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesColorSchemeCombo"));
    auto* remove = dialog->findChild<QPushButton*>(QStringLiteral("preferencesDeleteThemeButton"));
    CHECK(combo != nullptr && remove != nullptr);
    if (combo == nullptr || remove == nullptr) {
      dialog->reject();
      return;
    }
    const auto index = combo->findData(QStringLiteral("custom:doomed.patchytheme"));
    CHECK(index >= 0);
    combo->setCurrentIndex(index);
    QApplication::processEvents();
    CHECK(patchy::ui::theme().window_bg == QColor(70, 80, 90));

    QTimer::singleShot(0, [&] {
      auto* confirm = qobject_cast<QMessageBox*>(find_top_level_dialog(QStringLiteral("preferencesDeleteThemeConfirm")));
      CHECK(confirm != nullptr);
      if (confirm == nullptr) {
        return;
      }
      saw_confirm = true;
      for (auto* button : confirm->buttons()) {
        if (confirm->buttonRole(button) == QMessageBox::AcceptRole) {
          button->click();
          return;
        }
      }
      CHECK(false);
    });
    remove->click();
    QApplication::processEvents();

    CHECK(!QFileInfo::exists(path));
    CHECK(combo->findData(QStringLiteral("custom:doomed.patchytheme")) < 0);
    // No user entry left, so its separator went too: the built-ins, one
    // separator, and the bundled set remain.
    CHECK(combo->count() == 4 + patchy::ui::bundled_theme_file_names().size());
    CHECK(combo->currentIndex() == 0);
    CHECK(!remove->isEnabled());
    CHECK(!patchy::ui::has_active_custom_palette());
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
  CHECK(saw_confirm);
  CHECK(!patchy::ui::has_active_custom_palette());
  CHECK(!patchy::ui::ThemeManager::instance().active_custom_theme_id().has_value());
}

// Every bundled theme (themes.qrc) parses cleanly with no unknown-role warning
// (the generator writes every role, so a warning means the palette and the
// generated files drifted), lists in Preferences under a "bundled:" id right
// after the built-in entries, and cannot be deleted.
void ui_bundled_themes_load_and_list_in_preferences() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  ThemesDirEnvGuard themes_dir;  // an empty user folder, so only bundled entries follow the built-ins

  const auto names = patchy::ui::bundled_theme_file_names();
  CHECK(names.size() >= 5);
  for (const auto& name : names) {
    const auto id = patchy::ui::kBundledThemeIdPrefix + name;
    CHECK(patchy::ui::is_bundled_theme_id(id));
    const auto result = patchy::ui::load_theme_by_id(id);
    if (!result.theme) {
      fprintf(stderr, "  %s: %s\n", name.toUtf8().constData(), result.error.toUtf8().constData());
    }
    CHECK(result.theme.has_value());
    CHECK(result.error.isEmpty());
    CHECK(result.warnings.isEmpty());
    CHECK(!result.theme->name.isEmpty());
  }
  CHECK(!patchy::ui::load_theme_by_id(QStringLiteral("bundled:missing.patchytheme")).theme.has_value());

  patchy::ui::MainWindow window;
  show_window(window);
  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesColorSchemeCombo"));
    auto* remove = dialog->findChild<QPushButton*>(QStringLiteral("preferencesDeleteThemeButton"));
    CHECK(combo != nullptr && remove != nullptr);
    if (combo == nullptr || remove == nullptr) {
      dialog->reject();
      return;
    }
    // Three built-ins, one separator, then the bundled set in authored order.
    CHECK(combo->count() == 4 + names.size());
    for (int i = 0; i < names.size(); ++i) {
      CHECK(combo->itemData(4 + i).toString() == QStringLiteral("custom:bundled:") + names[i]);
    }
    save_widget_artifact("preferences_application_tab", *dialog);
    const auto index = combo->findData(QStringLiteral("custom:bundled:nord.patchytheme"));
    CHECK(index >= 0);
    combo->setCurrentIndex(index);
    QApplication::processEvents();
    CHECK(combo->currentText() == QStringLiteral("Nord (built-in)"));
    save_widget_artifact("preferences_application_tab_nord", *dialog);
    CHECK(patchy::ui::has_active_custom_palette());
    CHECK(patchy::ui::theme().window_bg == QColor(0x2e, 0x34, 0x40));
    CHECK(!remove->isEnabled());
    saw_dialog = true;
    dialog->reject();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
  CHECK(!patchy::ui::has_active_custom_palette());
}

// Export opens in the themes folder with the shown theme's name, and a file
// saved there is listed and selected at once, beside the tagged built-in it
// was copied from, as a deletable user theme.
void ui_preferences_export_defaults_to_themes_folder_and_lists_the_copy() {
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  SettingsValueRestorer restore_custom_id(QStringLiteral("preferences/customThemeId"));
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  ThemesDirEnvGuard themes_dir;
  const auto names = patchy::ui::bundled_theme_file_names();

  patchy::ui::MainWindow window;
  show_window(window);
  bool saw_dialog = false;
  bool saw_save_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesColorSchemeCombo"));
    auto* export_button = dialog->findChild<QPushButton*>(QStringLiteral("preferencesExportThemeButton"));
    auto* remove = dialog->findChild<QPushButton*>(QStringLiteral("preferencesDeleteThemeButton"));
    CHECK(combo != nullptr && export_button != nullptr && remove != nullptr);
    if (combo == nullptr || export_button == nullptr || remove == nullptr) {
      dialog->reject();
      return;
    }
    combo->setCurrentIndex(combo->findData(QStringLiteral("custom:bundled:nord.patchytheme")));
    QApplication::processEvents();

    QTimer::singleShot(0, [&] {
      auto* save = qobject_cast<QFileDialog*>(find_top_level_dialog(QStringLiteral("exportThemeFileDialog")));
      CHECK(save != nullptr);
      if (save == nullptr) {
        return;
      }
      saw_save_dialog = true;
      CHECK(QDir::cleanPath(save->directory().absolutePath()) == QDir::cleanPath(QDir(themes_dir.path()).absolutePath()));
      const auto selected = save->selectedFiles();
      CHECK(!selected.isEmpty());
      CHECK(!selected.isEmpty() && QFileInfo(selected.first()).fileName() == QStringLiteral("Nord.patchytheme"));
      save->selectFile(QDir(themes_dir.path()).filePath(QStringLiteral("Nord.patchytheme")));
      static_cast<QDialog*>(save)->accept();  // QFileDialog::accept is protected
    });
    export_button->click();
    QApplication::processEvents();

    const auto copy_path = QDir(themes_dir.path()).filePath(QStringLiteral("Nord.patchytheme"));
    CHECK(QFileInfo::exists(copy_path));
    // Three built-ins, a separator, the bundled set, a separator, the one user file.
    CHECK(combo->count() == 6 + names.size());
    CHECK(combo->currentData().toString() == QStringLiteral("custom:Nord.patchytheme"));
    CHECK(combo->currentText() == QStringLiteral("Nord"));
    CHECK(combo->findData(QStringLiteral("custom:bundled:nord.patchytheme")) >= 0);
    CHECK(remove->isEnabled());
    CHECK(patchy::ui::theme().window_bg == QColor(0x2e, 0x34, 0x40));
    const auto reread = patchy::ui::load_theme_by_id(QStringLiteral("Nord.patchytheme"));
    CHECK(reread.theme.has_value());
    CHECK(reread.theme.has_value() && reread.theme->name == QStringLiteral("Nord"));
    saw_dialog = true;
    dialog->reject();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
  CHECK(saw_save_dialog);
}

// A persisted bundled id needs no user folder at all (wasm has none), so it
// reapplies at startup through the resource path.
void ui_bundled_theme_id_persists_and_reapplies_like_a_restart() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  SettingsValueRestorer restore_scheme(QStringLiteral("preferences/colorScheme"));
  SettingsValueRestorer restore_custom_id(QStringLiteral("preferences/customThemeId"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/colorScheme"), QStringLiteral("dark"));
    settings.setValue(QStringLiteral("preferences/customThemeId"), QStringLiteral("bundled:dracula.patchytheme"));
    settings.sync();
  }
  patchy::ui::ThemeManager::instance().load_saved_preference();
  const auto active_id = patchy::ui::ThemeManager::instance().active_custom_theme_id();
  CHECK(active_id.has_value());
  CHECK(*active_id == QStringLiteral("bundled:dracula.patchytheme"));
  CHECK(patchy::ui::theme().window_bg == QColor(0x28, 0x2a, 0x36));
}

// The regression guard for "live, no restart": an already-built window has to
// restyle in place, and flipping back has to land exactly where it started.
void ui_color_scheme_switch_updates_existing_window() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);

  patchy::ui::MainWindow window;
  show_window(window);
  QApplication::processEvents();

  const auto dark_sheet = window.styleSheet();
  const auto dark_shot = window.grab().toImage();
  CHECK(dark_sheet.contains(patchy::ui::dark_palette().window_bg.name(QColor::HexRgb)));
  CHECK(!dark_shot.isNull());

  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Light);
  QApplication::processEvents();

  const auto light_sheet = window.styleSheet();
  CHECK(light_sheet != dark_sheet);
  CHECK(light_sheet.contains(patchy::ui::light_palette().window_bg.name(QColor::HexRgb)));
  CHECK(window.grab().toImage() != dark_shot);
  // The light variants of the stylesheet-referenced SVGs are a separate
  // mechanism from the icon engine, and the easiest one to forget.
  CHECK(light_sheet.contains(QStringLiteral("icons/light/scroll-dither.svg")));
  CHECK(dark_sheet.contains(QStringLiteral("icons/scroll-dither.svg")));
  CHECK(!dark_sheet.contains(QStringLiteral("icons/light/")));

  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);
  QApplication::processEvents();
  CHECK(window.styleSheet() == dark_sheet);
}

// Icons resolve their colors when painted, so the SAME QIcon a QAction was given
// at startup must render the new scheme after a flip. Capturing it by value up
// front is the whole point of the assertion: if this ever needs a setIcon call,
// the engine has regressed.
void ui_themed_icons_recolor_between_schemes() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Dark);

  patchy::ui::MainWindow window;
  show_window(window);

  const QIcon action_icon = require_action(window, "layerNewAction")->icon();
  CHECK(!action_icon.isNull());
  const auto dark_render = action_icon.pixmap(QSize(32, 32)).toImage();
  CHECK(!dark_render.isNull());

  // The paint swatches depict black and white. Their outlines are icon ink and
  // SHOULD recolor so the glyph stays visible on a light toolbar; only the two
  // fills are the subject of the drawing and must not move.
  const QPoint white_fill(23, 23);
  const QPoint black_fill(13, 13);
  const auto dark_swatch =
      patchy::ui::themed_svg_icon(QStringLiteral("default-colors")).pixmap(QSize(32, 32)).toImage();
  CHECK(dark_swatch.pixelColor(white_fill) == QColor(Qt::white));
  CHECK(dark_swatch.pixelColor(black_fill) == QColor(0x11, 0x11, 0x11));

  // Glyphs with no authored SVG go through a second engine that resolves a
  // palette role instead of substituting SVG text. The clipping badge is the one
  // users notice, and it is the reason that overload exists.
  const QIcon clip_icon =
      patchy::ui::simple_icon(QStringLiteral("clip"), &patchy::ui::ThemePalette::layer_clip_badge);
  const auto dark_clip = clip_icon.pixmap(QSize(20, 20)).toImage();
  CHECK(!dark_clip.isNull());

  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Light);

  const auto light_render = action_icon.pixmap(QSize(32, 32)).toImage();
  CHECK(light_render != dark_render);

  const auto ink_luminance = [](const QImage& image) {
    qint64 total = 0;
    qint64 covered = 0;
    for (int y = 0; y < image.height(); ++y) {
      for (int x = 0; x < image.width(); ++x) {
        const auto pixel = image.pixelColor(x, y);
        if (pixel.alpha() < 128) {
          continue;
        }
        total += qGray(pixel.rgb());
        ++covered;
      }
    }
    return covered > 0 ? total / covered : qint64{0};
  };
  // Light-on-dark ink becomes dark-on-light ink.
  CHECK(ink_luminance(light_render) < ink_luminance(dark_render));

  const auto light_swatch =
      patchy::ui::themed_svg_icon(QStringLiteral("default-colors")).pixmap(QSize(32, 32)).toImage();
  CHECK(light_swatch.pixelColor(white_fill) == QColor(Qt::white));
  CHECK(light_swatch.pixelColor(black_fill) == QColor(0x11, 0x11, 0x11));
  // ...while the outline around them did follow the scheme.
  CHECK(light_swatch != dark_swatch);

  // Same QIcon instance, so the role really is resolved at paint time.
  const auto light_clip = clip_icon.pixmap(QSize(20, 20)).toImage();
  CHECK(light_clip != dark_clip);
  CHECK(ink_luminance(light_clip) < ink_luminance(dark_clip));
}

void ui_main_window_persists_window_geometry() {
  SettingsValueRestorer restore_geometry(QStringLiteral("window/normalGeometry"));
  SettingsValueRestorer restore_maximized(QStringLiteral("window/maximized"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("window/normalGeometry"));
    settings.remove(QStringLiteral("window/maximized"));
    settings.sync();
  }

  // Derive the target from the available screen so the on-screen clamp performed during restore is
  // an identity operation; otherwise a small offscreen test screen would shrink/move the geometry.
  const QScreen* primary = QApplication::primaryScreen();
  CHECK(primary != nullptr);
  const QRect available = primary->availableGeometry();
  CHECK(available.isValid());
  const QRect target = available.adjusted(20, 20, -120, -100);
  CHECK(target.width() > 0 && target.height() > 0);
  {
    patchy::ui::MainWindow window;
    window.show();
    QApplication::processEvents();
    window.setGeometry(target);
    QApplication::processEvents();
    // Closing a window with no modified documents accepts the close and persists geometry.
    window.close();
    QApplication::processEvents();
  }

  QRect stored;
  {
    auto settings = patchy::ui::app_settings();
    stored = settings.value(QStringLiteral("window/normalGeometry")).toRect();
    CHECK(stored.isValid());
    CHECK(stored.size() == target.size());
    CHECK(!settings.value(QStringLiteral("window/maximized"), false).toBool());
  }

  patchy::ui::MainWindow restored;
  restored.show();
  QApplication::processEvents();
  CHECK(restored.size() == stored.size());
}

void ui_update_preference_defaults_startup_check_setting_to_enabled() {
  SettingsValueRestorer restore_update_check(QStringLiteral("updates/checkOnStartup"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("updates/checkOnStartup"));
    settings.sync();
  }

  auto settings = patchy::ui::app_settings();
  CHECK(!settings.contains(QStringLiteral("updates/checkOnStartup")));
  CHECK(settings.value(QStringLiteral("updates/checkOnStartup"), true).toBool());
}

void ui_psd_import_warning_preference_defaults_to_hidden() {
  SettingsValueRestorer restore_psd_warning_check(QStringLiteral("imports/showPsdWarningsAndInfo"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("imports/showPsdWarningsAndInfo"));
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* check = dialog->findChild<QCheckBox*>(QStringLiteral("preferencesShowPsdImportWarningsCheck"));
    CHECK(check != nullptr);
    CHECK(check->text() == QStringLiteral("Show import warnings and notes in a popup (status bar otherwise)"));
    CHECK(!check->isChecked());
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
}

void ui_psd_import_warning_preference_persists_enabled_setting() {
  SettingsValueRestorer restore_psd_warning_check(QStringLiteral("imports/showPsdWarningsAndInfo"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("imports/showPsdWarningsAndInfo"));
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* check = dialog->findChild<QCheckBox*>(QStringLiteral("preferencesShowPsdImportWarningsCheck"));
    CHECK(check != nullptr);
    CHECK(!check->isChecked());
    check->setChecked(true);
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);

  auto settings = patchy::ui::app_settings();
  CHECK(settings.value(QStringLiteral("imports/showPsdWarningsAndInfo"), false).toBool());
}

void ui_transform_shift_aspect_preference_defaults_to_off() {
  SettingsValueRestorer restore_shift_aspect(QStringLiteral("input/shiftKeepsTransformAspect"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("input/shiftKeepsTransformAspect"));
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  CHECK(!canvas->shift_keeps_transform_aspect());

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* check = dialog->findChild<QCheckBox*>(QStringLiteral("preferencesTransformShiftAspectCheck"));
    CHECK(check != nullptr);
    CHECK(check->text() == QStringLiteral("Hold Shift to keep the aspect ratio when transforming"));
    CHECK(!check->isChecked());
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
}

void ui_transform_shift_aspect_preference_persists_and_reaches_canvas() {
  SettingsValueRestorer restore_shift_aspect(QStringLiteral("input/shiftKeepsTransformAspect"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("input/shiftKeepsTransformAspect"));
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* check = dialog->findChild<QCheckBox*>(QStringLiteral("preferencesTransformShiftAspectCheck"));
    CHECK(check != nullptr);
    CHECK(!check->isChecked());
    check->setChecked(true);
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);

  auto settings = patchy::ui::app_settings();
  CHECK(settings.value(QStringLiteral("input/shiftKeepsTransformAspect"), false).toBool());
  // Accepting the dialog pushes the new value onto every open canvas.
  CHECK(canvas->shift_keeps_transform_aspect());
}

// input/snapTransformsToPixelGrid defaults on (Photoshop's default) and reaches the canvas.
void ui_transform_snap_preference_persists_and_reaches_canvas() {
  SettingsValueRestorer restore_snap(QStringLiteral("input/snapTransformsToPixelGrid"));
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("input/snapTransformsToPixelGrid"));
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window(window);
  auto* canvas = require_canvas(window);
  CHECK(canvas->snap_transforms_to_pixel_grid());

  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* check = dialog->findChild<QCheckBox*>(QStringLiteral("preferencesTransformSnapToPixelGridCheck"));
    CHECK(check != nullptr);
    CHECK(check->isChecked());
    check->setChecked(false);
    saw_dialog = true;
    dialog->accept();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);

  auto settings = patchy::ui::app_settings();
  CHECK(!settings.value(QStringLiteral("input/snapTransformsToPixelGrid"), true).toBool());
  CHECK(!canvas->snap_transforms_to_pixel_grid());
}

void ui_language_switch_updates_existing_window() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("documentTabs"));
  CHECK(tabs != nullptr);
  const auto initial_tab_count = tabs->count();

  choose_preferences_language(window, QStringLiteral("ja"));

  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("ja"));
  const auto japanese_menus = top_level_menu_texts(*window.menuBar());
  CHECK(japanese_menus.contains(QStringLiteral("ファイル(F)")));
  CHECK(!japanese_menus.contains(QStringLiteral("環境設定(P)")));
  CHECK(tabs->count() == initial_tab_count);
  CHECK(require_action(window, "helpAiSetupAction")->text() ==
        QStringLiteral("AI制御のセットアップ(&U)..."));

  choose_preferences_language(window, QStringLiteral("en"));

  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("en"));
  const auto english_menus = top_level_menu_texts(*window.menuBar());
  CHECK(english_menus.contains(QStringLiteral("File")));
  CHECK(!english_menus.contains(QStringLiteral("Preferences")));
  CHECK(require_action(window, "fileSaveAction")->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_S));
  CHECK(require_action(window, "helpAiSetupAction")->text() ==
        QStringLiteral("Set &up AI Control..."));
  CHECK(tabs->count() == initial_tab_count);
}

void ui_language_switch_survives_window_reactivation() {
  // GitHub issue 29. On macOS Qt merges a menubar item whose title starts with its translated
  // "Setting", "Setup", "Options", "About", "Quit"... into the application menu. A runtime switch
  // to Spanish or French renamed Image > Adjustments to "Ajustes"/"Réglages" (Chinese renames
  // Window > Set Screen Size), Qt swapped that submenu's native item for the merged one and freed
  // the old one while the submenu still pointed at it, and the next key-window change crashed in
  // setSubmenu:. Offscreen has no native menubar: reproduce on a mac with
  // PATCHY_UI_TEST_PLATFORM=cocoa NSZombieEnabled=YES (zombies turn the message to the freed item
  // into an abort; in a short run the freed memory is often still intact and nothing shows).
  // ui_menubar_submenus_have_no_native_menu_role is the platform-independent guard.
  patchy::ui::MainWindow window;
  show_window(window);
  window.raise();
  window.activateWindow();
  process_events_for(150);
  for (const char* code : {"es", "fr", "zh_CN", "en"}) {
    choose_preferences_language(window, QString::fromLatin1(code));
    CHECK(patchy::ui::LocalizationManager::instance().current_language() == QString::fromLatin1(code));
    // Another window takes key, then the main window regains it: the menubar re-sync that crashed.
    QDialog other(&window);
    other.setObjectName(QStringLiteral("languageSwitchOtherWindow"));
    other.resize(200, 100);
    other.show();
    other.raise();
    other.activateWindow();
    process_events_for(150);
    other.close();
    window.raise();
    window.activateWindow();
    process_events_for(150);
  }
  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("en"));
  CHECK(top_level_menu_texts(*window.menuBar()).contains(QStringLiteral("Image")));
}

void ui_menubar_submenus_have_no_native_menu_role() {
  // Every submenu under the menubar opts out of Qt's macOS menu-role text heuristic; see
  // ui_language_switch_survives_window_reactivation for the crash a merged submenu causes.
  patchy::ui::MainWindow window;
  show_window(window);
  int submenus = 0;
  std::function<void(QMenu&)> walk = [&](QMenu& menu) {
    for (auto* action : menu.actions()) {
      auto* submenu = action->menu();
      if (submenu == nullptr) {
        continue;
      }
      ++submenus;
      if (action->menuRole() != QAction::NoRole) {
        fprintf(stderr, "submenu keeps a native menu role: %s\n", qPrintable(action->text()));
      }
      CHECK(action->menuRole() == QAction::NoRole);
      walk(*submenu);
    }
  };
  for (auto* action : window.menuBar()->actions()) {
    if (auto* menu = action->menu()) {
      walk(*menu);
    }
  }
  CHECK(submenus >= 20);
}

void ui_language_preference_applies_at_startup() {
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/language"), QStringLiteral("ja"));
    settings.sync();
  }
  patchy::ui::LocalizationManager::instance().load_saved_language();

  patchy::ui::MainWindow window;
  show_window(window);

  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("ja"));
  const auto menus = top_level_menu_texts(*window.menuBar());
  CHECK(menus.contains(QStringLiteral("ファイル(F)")));
  CHECK(!menus.contains(QStringLiteral("環境設定(P)")));
}

void ui_language_missing_preference_uses_system_language() {
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("preferences/language"));
    settings.sync();
  }
  patchy::ui::LocalizationManager::instance().load_saved_language(QLocale(QLocale::Japanese, QLocale::Japan));

  patchy::ui::MainWindow window;
  show_window(window);

  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("ja"));
  const auto menus = top_level_menu_texts(*window.menuBar());
  CHECK(menus.contains(QStringLiteral("ファイル(F)")));
  auto settings = patchy::ui::app_settings();
  CHECK(!settings.contains(QStringLiteral("preferences/language")));
}

void ui_language_saved_preference_overrides_system_language() {
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/language"), QStringLiteral("en"));
    settings.sync();
  }
  patchy::ui::LocalizationManager::instance().load_saved_language(QLocale(QLocale::Japanese, QLocale::Japan));

  patchy::ui::MainWindow window;
  show_window(window);

  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("en"));
  const auto menus = top_level_menu_texts(*window.menuBar());
  CHECK(menus.contains(QStringLiteral("File")));
  auto settings = patchy::ui::app_settings();
  CHECK(settings.value(QStringLiteral("preferences/language")).toString() == QStringLiteral("en"));
  for (const auto* tag : {"pt_PT", "ru_RU", "pl_PL", "ko_KR"}) {
    patchy::ui::LocalizationManager::instance().load_saved_language(QLocale(QString::fromLatin1(tag)));
    CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("en"));
  }
}

void ui_language_invalid_preference_falls_back_to_english() {
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("preferences/language"), QStringLiteral("zz"));
    settings.sync();
  }
  patchy::ui::LocalizationManager::instance().load_saved_language();

  patchy::ui::MainWindow window;
  show_window(window);

  CHECK(patchy::ui::LocalizationManager::instance().current_language() == QStringLiteral("en"));
  const auto menus = top_level_menu_texts(*window.menuBar());
  CHECK(menus.contains(QStringLiteral("File")));
  CHECK(!menus.contains(QStringLiteral("Preferences")));
}

void ui_language_matching_maps_locales_to_shipped_codes() {
  auto& manager = patchy::ui::LocalizationManager::instance();
  // Plain codes, locale names and BCP 47 tags all resolve to a catalog code.
  CHECK(manager.match_language(QStringLiteral("en")) == QStringLiteral("en"));
  CHECK(manager.match_language(QStringLiteral("en_US")) == QStringLiteral("en"));
  CHECK(manager.match_language(QStringLiteral("ja_JP")) == QStringLiteral("ja"));
  CHECK(manager.match_language(QStringLiteral("fr-CA")) == QStringLiteral("fr"));
  CHECK(manager.match_language(QStringLiteral("de_AT")) == QStringLiteral("de"));
  CHECK(manager.match_language(QStringLiteral("es-MX")) == QStringLiteral("es"));
  CHECK(manager.match_language(QStringLiteral("it")) == QStringLiteral("it"));
  // Chinese picks the variant by script: Traditional for Taiwan, Hong Kong and Macao
  // or an explicit Hant tag, Simplified otherwise.
  CHECK(manager.match_language(QStringLiteral("zh_CN")) == QStringLiteral("zh_CN"));
  CHECK(manager.match_language(QStringLiteral("zh")) == QStringLiteral("zh_CN"));
  CHECK(manager.match_language(QStringLiteral("zh_SG")) == QStringLiteral("zh_CN"));
  CHECK(manager.match_language(QStringLiteral("zh-Hans-CN")) == QStringLiteral("zh_CN"));
  CHECK(manager.match_language(QStringLiteral("zh_TW")) == QStringLiteral("zh_TW"));
  CHECK(manager.match_language(QStringLiteral("zh_HK")) == QStringLiteral("zh_TW"));
  CHECK(manager.match_language(QStringLiteral("zh-Hant")) == QStringLiteral("zh_TW"));
  for (const auto* tag : {"pt", "pt_BR", "pt-BR", "pt-PT", "pt-Latn-BR"}) {
    CHECK(manager.match_language(QString::fromLatin1(tag)) == QStringLiteral("pt_BR"));
  }
  CHECK(manager.match_language(QStringLiteral("ru-RU")) == QStringLiteral("ru"));
  CHECK(manager.match_language(QStringLiteral("pl_PL")) == QStringLiteral("pl"));
  CHECK(manager.match_language(QStringLiteral("ko-KR")) == QStringLiteral("ko"));
  // Languages Patchy does not ship match nothing; the caller falls back to English.
  CHECK(manager.match_language(QStringLiteral("zz")).isEmpty());
  CHECK(manager.match_language(QString()).isEmpty());

  CHECK(manager.language_for_locale(QLocale(QLocale::French, QLocale::Canada)) == QStringLiteral("fr"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Chinese, QLocale::TraditionalHanScript, QLocale::Taiwan)) ==
        QStringLiteral("zh_TW"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Chinese, QLocale::SimplifiedHanScript, QLocale::China)) ==
        QStringLiteral("zh_CN"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Portuguese, QLocale::Brazil)) == QStringLiteral("pt_BR"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Portuguese, QLocale::Portugal)) == QStringLiteral("pt_BR"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Russian, QLocale::Russia)) == QStringLiteral("ru"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Polish, QLocale::Poland)) == QStringLiteral("pl"));
  CHECK(manager.language_for_locale(QLocale(QLocale::Korean, QLocale::SouthKorea)) == QStringLiteral("ko"));
  CHECK(manager.language_for_locale(QLocale::c()) == QStringLiteral("en"));

  // Selecting an unshipped language reports failure and leaves English active.
  CHECK(!manager.set_language(QStringLiteral("nl_NL"), false));
  CHECK(manager.current_language() == QStringLiteral("en"));
}

void ui_language_combo_lists_every_shipped_language() {
  auto& manager = patchy::ui::LocalizationManager::instance();
  const auto& shipped = manager.languages();
  CHECK(!shipped.empty());
  CHECK(shipped.front().code == QStringLiteral("en"));
  // Every shipped catalog is built next to the test binary, so the installed set is
  // the full table.
  const auto available = manager.available_languages();
  CHECK(available.size() == shipped.size());

  patchy::ui::MainWindow window;
  show_window(window);
  auto* preferences = require_action(window, "filePreferencesAction");
  bool saw_dialog = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("preferencesLanguageCombo"));
    CHECK(combo != nullptr);
    CHECK(combo->count() == static_cast<int>(shipped.size()));
    for (int index = 0; index < combo->count(); ++index) {
      const auto& language = shipped[static_cast<std::size_t>(index)];
      CHECK(combo->itemData(index).toString() == language.code);
      CHECK(combo->itemText(index) == language.native_name);
    }
    CHECK(combo->currentData().toString() == manager.current_language());
    saw_dialog = true;
    dialog->reject();
  });
  preferences->trigger();
  QApplication::processEvents();
  CHECK(saw_dialog);
}

void ui_language_every_catalog_loads_and_translates() {
  auto& manager = patchy::ui::LocalizationManager::instance();
  for (const auto& language : manager.languages()) {
    CHECK(manager.set_language(language.code, false));
    CHECK(manager.current_language() == language.code);
    // Two everyday strings that differ from English in every shipped language, so a
    // match means the catalog did not load and Qt fell back to the source text. "&File"
    // would not work here: Italian keeps "File", the way Adobe's Italian UI does.
    const auto layer_menu = QCoreApplication::translate("patchy::ui::MainWindow", "&Layer");
    const auto ready = QCoreApplication::translate("patchy::ui::MainWindow", "Ready");
    if (language.code == QStringLiteral("en")) {
      CHECK(layer_menu == QStringLiteral("&Layer"));
      CHECK(ready == QStringLiteral("Ready"));
    } else {
      CHECK(layer_menu != QStringLiteral("&Layer"));
      CHECK(ready != QStringLiteral("Ready"));
    }
  }
  CHECK(manager.set_language(QStringLiteral("en"), false));
}

void ui_language_catalog_covers_dialog_status_and_properties() {
  CHECK(patchy::ui::LocalizationManager::instance().set_language(QStringLiteral("ja"), false));
  QApplication::processEvents();

  const auto canvas_status = QCoreApplication::translate(
      "patchy::ui::CanvasWidget", "Select a normal pixel layer before painting on text");
  CHECK(canvas_status == QStringLiteral("テキスト上に描画する前に通常のピクセルレイヤーを選択してください"));

  const auto save_title = QCoreApplication::translate("patchy::ui::MainWindow", "Save changes?");
  CHECK(save_title == QStringLiteral("変更を保存しますか?"));
  const auto save_prompt =
      QCoreApplication::translate("patchy::ui::MainWindow", "Save changes to %1 before closing?");
  CHECK(save_prompt == QStringLiteral("閉じる前に %1 への変更を保存しますか?"));

  const auto no_layer = QCoreApplication::translate("patchy::ui::MainWindow", "Layer: No active layer");
  CHECK(no_layer == QStringLiteral("レイヤー: アクティブレイヤーなし"));
  const auto document_info = QCoreApplication::translate(
      "patchy::ui::MainWindow", "Document: %1 x %2 px | %3 x %4 %5 | %6 ppi | %7 | %8 layers | Zoom %9% | %10");
  CHECK(document_info.startsWith(QStringLiteral("ドキュメント:")));
  const auto bmp_depth = QCoreApplication::translate("QObject", "Color depth");
  CHECK(bmp_depth == QStringLiteral("色深度"));
  const auto bmp_quantize = QCoreApplication::translate("QObject", "Reduce colors automatically");
  CHECK(bmp_quantize == QStringLiteral("色数を自動的に減らす"));
  const auto bmp_palette_file = QCoreApplication::translate("QObject", "Use palette file");
  CHECK(bmp_palette_file == QStringLiteral("パレットファイルを使用"));
  const auto settings_file = QCoreApplication::translate("QObject", "Settings file:");
  CHECK(settings_file == QStringLiteral("設定ファイル:"));
  const auto open_settings_folder = QCoreApplication::translate("QObject", "Open Settings Folder");
  CHECK(open_settings_folder == QStringLiteral("設定フォルダーを開く"));
  const auto settings_folder_failed = QCoreApplication::translate("QObject", "Could not open settings folder.");
  CHECK(settings_folder_failed == QStringLiteral("設定フォルダーを開けませんでした。"));
  const auto data_folder = QCoreApplication::translate("QObject", "User data folder (fonts, scripts):");
  CHECK(data_folder == QStringLiteral("ユーザーデータフォルダー (フォント、スクリプト):"));
  const auto open_data_folder = QCoreApplication::translate("QObject", "Open Data Folder");
  CHECK(open_data_folder == QStringLiteral("データフォルダーを開く"));
  const auto data_folder_failed = QCoreApplication::translate("QObject", "Could not open data folder.");
  CHECK(data_folder_failed == QStringLiteral("データフォルダーを開けませんでした。"));

  // The About dialog's folder rows must translate in every shipped language, not only the
  // Japanese pins above: a string that comes back as its English source means the entry is
  // missing from that catalog or carries the wrong context.
  auto& manager = patchy::ui::LocalizationManager::instance();
  for (const auto& language : manager.languages()) {
    if (language.code == QStringLiteral("en")) {
      continue;
    }
    CHECK(manager.set_language(language.code, false));
    QApplication::processEvents();
    for (const char* source : {"Settings file:", "Open Settings Folder", "Could not open settings folder.",
                               "User data folder (fonts, scripts):", "Open Data Folder",
                               "Could not open data folder."}) {
      const auto translated = QCoreApplication::translate("QObject", source);
      if (translated == QString::fromUtf8(source)) {
        std::cerr << "untranslated in " << language.code.toStdString() << ": " << source << "\n";
      }
      CHECK(translated != QString::fromUtf8(source));
    }
  }
  CHECK(manager.set_language(QStringLiteral("ja"), false));
  QApplication::processEvents();
  const auto checking_updates = QCoreApplication::translate("QObject", "Checking for updates...");
  CHECK(checking_updates == QStringLiteral("更新を確認しています..."));
  const auto up_to_date = QCoreApplication::translate("QObject", "Patchy is up to date (%1).");
  CHECK(up_to_date == QStringLiteral("Patchy は最新です (%1)。"));
  const auto update_failed =
      QCoreApplication::translate("QObject", "Update check failed: invalid update manifest.");
  CHECK(update_failed == QStringLiteral("更新確認に失敗しました: 更新マニフェストが無効です。"));
  const auto show_effects = QCoreApplication::translate("QObject", "Show Effects");
  CHECK(show_effects == QStringLiteral("効果を表示"));
  const auto gloss_contour = QCoreApplication::translate("QObject", "Gloss Contour");
  CHECK(gloss_contour == QStringLiteral("光沢輪郭"));
  const auto link_with_layer = QCoreApplication::translate("QObject", "Link with Layer");
  CHECK(link_with_layer == QStringLiteral("レイヤーにリンク"));
  const auto pattern_basketweave = QCoreApplication::translate("QObject", "Basketweave");
  CHECK(pattern_basketweave == QStringLiteral("バスケット編み"));
  const auto contour_ring_double = QCoreApplication::translate("QObject", "Ring - Double");
  CHECK(contour_ring_double == QStringLiteral("リング - 二重"));
  const auto curves_graph = QCoreApplication::translate("QObject", "Curves graph");
  CHECK(curves_graph == QStringLiteral("トーンカーブグラフ"));
  const auto curves_input = QCoreApplication::translate("QObject", "Input:");
  CHECK(curves_input == QStringLiteral("入力:"));
  const auto curves_auto =
      QCoreApplication::translate("QObject", "Set the active channel from its histogram");
  CHECK(curves_auto == QStringLiteral("ヒストグラムに基づいて選択中のチャンネルを自動調整"));
  const auto curves_summary =
      QCoreApplication::translate("QObject", "Curves: RGB %1, Red %2, Green %3, Blue %4 points");
  CHECK(curves_summary == QStringLiteral("トーンカーブ: RGB %1、赤 %2、緑 %3、青 %4 ポイント"));
  const auto curves_presets = QCoreApplication::translate("QObject", "Curves presets");
  CHECK(curves_presets == QStringLiteral("トーンカーブのプリセット"));
  const auto medium_contrast = QCoreApplication::translate("QObject", "Medium Contrast");
  CHECK(medium_contrast == QStringLiteral("中程度のコントラスト"));
  const auto soft_glow = QCoreApplication::translate("QObject", "Soft Glow");
  CHECK(soft_glow == QStringLiteral("ソフトグロー"));
  const auto filter_radius = QCoreApplication::translate("QObject", "Radius");
  CHECK(filter_radius == QStringLiteral("半径"));
  CHECK(patchy::ui::filter_progress_stage_text(patchy::FilterProgressStage::GeneratingClouds) ==
        QStringLiteral("雲模様を生成しています"));
  const auto filter_gallery_action =
      QCoreApplication::translate("patchy::ui::MainWindow", "Filter &Gallery...");
  CHECK(filter_gallery_action == QStringLiteral("フィルターギャラリー(&G)..."));
  const auto filter_gallery_tip = QCoreApplication::translate(
      "patchy::ui::MainWindow", "Preview and apply visual filters and photo looks");
  CHECK(filter_gallery_tip == QStringLiteral("ビジュアルフィルターとフォトルックをプレビューして適用"));
  const auto filter_gallery_cancelled =
      QCoreApplication::translate("patchy::ui::MainWindow", "Cancelled Filter Gallery");
  CHECK(filter_gallery_cancelled == QStringLiteral("フィルターギャラリーをキャンセルしました"));
  const auto filter_gallery_none =
      QCoreApplication::translate("patchy::ui::MainWindow", "No visual filter applied");
  CHECK(filter_gallery_none == QStringLiteral("ビジュアルフィルターは適用されませんでした"));
  const auto filter_gallery_title = QCoreApplication::translate("QObject", "Filter Gallery");
  CHECK(filter_gallery_title == QStringLiteral("フィルターギャラリー"));
  const auto filter_gallery_original = QCoreApplication::translate("QObject", "Original");
  CHECK(filter_gallery_original == QStringLiteral("元画像"));
  const auto filter_gallery_canvas = QCoreApplication::translate("QObject", "Live Canvas Preview");
  CHECK(filter_gallery_canvas == QStringLiteral("キャンバスでライブプレビュー"));
  const auto filter_gallery_apply = QCoreApplication::translate("QObject", "Apply");
  CHECK(filter_gallery_apply == QStringLiteral("適用"));
  const auto filter_gallery_rendering = QCoreApplication::translate("QObject", "Rendering preview...");
  CHECK(filter_gallery_rendering == QStringLiteral("プレビューを描画しています..."));
  const auto filter_gallery_ready = QCoreApplication::translate("QObject", "Ready");
  CHECK(filter_gallery_ready == QStringLiteral("準備完了"));
  const auto curves_load = QCoreApplication::translate("QObject", "Load...");
  CHECK(curves_load == QStringLiteral("読み込み..."));
  const auto curves_save = QCoreApplication::translate("QObject", "Save...");
  CHECK(curves_save == QStringLiteral("保存..."));
  const auto curves_load_title = QCoreApplication::translate("QObject", "Load Curves Preset");
  CHECK(curves_load_title == QStringLiteral("トーンカーブプリセットを読み込み"));
  const auto curves_save_title = QCoreApplication::translate("QObject", "Save Curves Preset");
  CHECK(curves_save_title == QStringLiteral("トーンカーブプリセットを保存"));
  const auto curves_preset_filter =
      QCoreApplication::translate("QObject", "Photoshop Curves Preset (*.acv)");
  CHECK(curves_preset_filter == QStringLiteral("Photoshop トーンカーブプリセット (*.acv)"));
  const auto curves_load_error = QCoreApplication::translate(
      "QObject", "The Curves preset could not be loaded. The file may be damaged or unsupported.");
  CHECK(curves_load_error == QStringLiteral(
                                  "トーンカーブプリセットを読み込めませんでした。ファイルが破損しているか、対応していない可能性があります。"));
  const auto curves_save_error =
      QCoreApplication::translate("QObject", "The Curves preset could not be saved.");
  CHECK(curves_save_error == QStringLiteral("トーンカーブプリセットを保存できませんでした。"));
  const auto curves_target = QCoreApplication::translate("QObject", "Target");
  CHECK(curves_target == QStringLiteral("画像内調整"));
  const auto curves_before = QCoreApplication::translate("QObject", "Before");
  CHECK(curves_before == QStringLiteral("調整前"));
  const auto curves_clipping =
      QCoreApplication::translate("QObject", "Show shadow and highlight clipping together");
  CHECK(curves_clipping == QStringLiteral("シャドウとハイライトのクリッピングを同時に表示"));

  CHECK(patchy::ui::LocalizationManager::instance().set_language(QStringLiteral("en"), false));
  QApplication::processEvents();
}

void ui_filter_gallery_action_retranslates() {
  CHECK(patchy::ui::LocalizationManager::instance().set_language(QStringLiteral("en"), false));
  patchy::ui::MainWindow window;
  show_window(window);
  auto* action = require_action(window, "filterGalleryAction");
  CHECK(action->text() == QStringLiteral("Filter &Gallery..."));
  CHECK(action->statusTip() == QStringLiteral("Preview and apply visual filters and photo looks"));

  CHECK(patchy::ui::LocalizationManager::instance().set_language(QStringLiteral("ja"), false));
  QApplication::processEvents();
  CHECK(action->text() == QStringLiteral("フィルターギャラリー(&G)..."));
  CHECK(action->statusTip() == QStringLiteral("ビジュアルフィルターとフォトルックをプレビューして適用"));
  patchy::ui::ZoomableImagePreview translated_preview;
  CHECK(translated_preview.toolTip() ==
        QStringLiteral("ドラッグで表示位置を移動できます。マウスホイールでズームします。"));

  CHECK(patchy::ui::LocalizationManager::instance().set_language(QStringLiteral("en"), false));
  QApplication::processEvents();
  CHECK(action->text() == QStringLiteral("Filter &Gallery..."));
  CHECK(action->statusTip() == QStringLiteral("Preview and apply visual filters and photo looks"));
}

void ui_about_dialog_shows_labeled_external_links() {
  bool inspected = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchySplashScreen"));
    CHECK(dialog != nullptr);

    const auto link_labels = dialog->findChildren<QLabel*>(QStringLiteral("splashHome"));
    CHECK(link_labels.size() == 2);
    QString combined_text;
    for (const auto* label : link_labels) {
      CHECK(label->textFormat() == Qt::RichText);
      CHECK(label->textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse));
      CHECK(label->openExternalLinks());
      combined_text += label->text();
      combined_text += QLatin1Char('\n');
    }

    CHECK(combined_text.contains(QStringLiteral("GitHub: ")));
    CHECK(combined_text.contains(QStringLiteral("href=\"https://github.com/SethRobinson/Patchy\"")));
    CHECK(combined_text.contains(QStringLiteral(">SethRobinson/Patchy</a>")));
    CHECK(combined_text.contains(QStringLiteral("Seth's site: ")));
    CHECK(combined_text.contains(QStringLiteral("href=\"https://rtsoft.com\"")));
    CHECK(combined_text.contains(QStringLiteral(">rtsoft.com</a>")));

    // A link color lives inside the rich text, where the stylesheet token check
    // cannot see it. An unresolved one is not an error to Qt, just an unreadable
    // color: the parser drops it and falls back to its own blue.
    CHECK(!combined_text.contains(QLatin1Char('@')));
    CHECK(combined_text.contains(
        QStringLiteral("color:%1;").arg(patchy::ui::theme().splash_link_text.name(QColor::HexRgb))));

    // The version label shares the splashCredit object name with the credit
    // line below it; creation order puts the version first.
    const auto credit_labels = dialog->findChildren<QLabel*>(QStringLiteral("splashCredit"));
    CHECK(credit_labels.size() == 2);
    CHECK(credit_labels.first()->text().startsWith(QStringLiteral("Version ")));
    CHECK(credit_labels.first()->text().endsWith(
        QStringLiteral("(built %1)").arg(patchy::ui::build_timestamp_text())));
    // The credit names Seth and links to his GitHub profile in the themed link color.
    auto* credit = credit_labels.last();
    CHECK(credit->textFormat() == Qt::RichText);
    CHECK(credit->openExternalLinks());
    CHECK(credit->text().startsWith(QStringLiteral("Created by ")));
    CHECK(credit->text().contains(QStringLiteral("href=\"https://github.com/SethRobinson\"")));
    CHECK(credit->text().contains(QStringLiteral(">Seth A. Robinson</a>")));
    CHECK(!credit->text().contains(QStringLiteral("@splash_link_text")));
    CHECK(credit->text().contains(
        QStringLiteral("color:%1;").arg(patchy::ui::theme().splash_link_text.name(QColor::HexRgb))));

    auto* contributors = dialog->findChild<QLabel*>(QStringLiteral("splashContributors"));
    CHECK(contributors != nullptr);
    CHECK(contributors->textFormat() == Qt::RichText);
    CHECK(contributors->openExternalLinks());
    CHECK(contributors->text().startsWith(QStringLiteral("Incredible people who donated suggestions, bug reports, and code: ")));
    CHECK(contributors->text().contains(QStringLiteral("href=\"https://github.com/mcapogna\"")));
    CHECK(contributors->text().contains(QStringLiteral(">mcapogna</a>")));
    CHECK(contributors->text().contains(QStringLiteral("href=\"https://github.com/csbun\"")));
    CHECK(contributors->text().contains(QStringLiteral(">csbun</a>")));
    CHECK(contributors->text().contains(QStringLiteral("href=\"https://github.com/ifloppy\"")));
    CHECK(contributors->text().contains(QStringLiteral(">ifloppy</a>")));
    CHECK(contributors->text().contains(QStringLiteral("href=\"https://github.com/lucastucious\"")));
    CHECK(contributors->text().contains(QStringLiteral(">lucastucious</a>")));
    CHECK(contributors->text().contains(QStringLiteral("href=\"https://github.com/Oppa-YA\"")));
    CHECK(contributors->text().contains(QStringLiteral(">Oppa-YA</a>")));
    CHECK(contributors->text().contains(QStringLiteral("href=\"https://github.com/YuFu-Onship\"")));
    CHECK(contributors->text().contains(QStringLiteral(">YuFu-Onship</a>")));
    CHECK(!contributors->text().contains(QLatin1Char('@')));

    auto* settings_caption = dialog->findChild<QLabel*>(QStringLiteral("splashSettingsCaption"));
    CHECK(settings_caption != nullptr);
    CHECK(settings_caption->text() == QStringLiteral("Settings file:"));
    auto* settings_path = dialog->findChild<QLabel*>(QStringLiteral("splashSettingsPath"));
    CHECK(settings_path != nullptr);
    CHECK(settings_path->text() == QDir::toNativeSeparators(patchy::ui::app_settings().fileName()));
    CHECK(settings_path->textInteractionFlags().testFlag(Qt::TextSelectableByMouse));
    CHECK(settings_path->wordWrap());
    auto* open_settings_folder = dialog->findChild<QPushButton*>(QStringLiteral("splashOpenSettingsFolderButton"));
    CHECK(open_settings_folder != nullptr);
    CHECK(open_settings_folder->text() == QStringLiteral("Open Settings Folder"));

    // The per-user data folder (dropped fonts, user scripts) gets the same treatment.
    auto* data_caption = dialog->findChild<QLabel*>(QStringLiteral("splashDataCaption"));
    CHECK(data_caption != nullptr);
    CHECK(data_caption->text() == QStringLiteral("User data folder (fonts, scripts):"));
    auto* data_path = dialog->findChild<QLabel*>(QStringLiteral("splashDataPath"));
    CHECK(data_path != nullptr);
    CHECK(data_path->text() == QDir::toNativeSeparators(
                                   QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
    CHECK(data_path->textInteractionFlags().testFlag(Qt::TextSelectableByMouse));
    CHECK(data_path->wordWrap());
    auto* open_data_folder = dialog->findChild<QPushButton*>(QStringLiteral("splashOpenDataFolderButton"));
    CHECK(open_data_folder != nullptr);
    CHECK(open_data_folder->text() == QStringLiteral("Open Data Folder"));

    // The dialog is as tall as its content at its fixed width; a shorter fixed height
    // squeezed every row on Windows until the first move relaid the column out.
    CHECK(dialog->layout() != nullptr);
    CHECK(dialog->height() >= dialog->layout()->totalHeightForWidth(dialog->width()));
    CHECK(dialog->minimumHeight() == dialog->maximumHeight());
    CHECK(open_data_folder->height() >= open_data_folder->sizeHint().height());
    CHECK(data_path->height() >= data_path->heightForWidth(data_path->width()));

    save_widget_artifact("ui_about_dialog_links", *dialog);
    inspected = true;
    dialog->accept();
  });

  patchy::ui::show_about_splash();
  CHECK(inspected);
}

// The organization name changed from "Seth A. Robinson" to "RTsoft" after 0.98, which
// moves AppDataLocation (user fonts, user scripts). Startup merges the old folder into the
// new one without overwriting, drops identical leftovers, keeps differing ones, and removes
// the emptied legacy tree down to the organization folder.
void ui_app_data_migration_merges_legacy_folder() {
  namespace migration = patchy::ui::app_data_migration;
  QTemporaryDir temp;
  CHECK(temp.isValid());
  const auto root = temp.path();
  const auto legacy = root + QStringLiteral("/Seth A. Robinson/Patchy");
  const auto current = root + QStringLiteral("/RTsoft/Patchy");
  const auto write = [](const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(bytes);
  };
  const auto read = [](const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
  };

  // No legacy folder: nothing happens and the current folder is not created.
  auto result = migration::migrate_app_data_directory(legacy, current);
  CHECK(!result.legacy_found);
  CHECK(!QDir(current).exists());

  write(legacy + QStringLiteral("/user-fonts/Dropped.ttf"), "font-bytes");
  write(legacy + QStringLiteral("/user-fonts/Same.ttf"), "same");
  write(legacy + QStringLiteral("/scripts/Demos/mine.js"), "legacy edit");
  write(legacy + QStringLiteral("/scripts/Empty/.keep"), "");
  QDir().mkpath(legacy + QStringLiteral("/scripts/Nothing"));
  write(current + QStringLiteral("/user-fonts/Same.ttf"), "same");
  write(current + QStringLiteral("/user-fonts/Newer.ttf"), "newer");
  write(current + QStringLiteral("/scripts/Demos/mine.js"), "current edit");

  result = migration::migrate_app_data_directory(legacy, current);
  CHECK(result.legacy_found);
  CHECK(result.files_moved == 2);        // Dropped.ttf and .keep
  CHECK(result.duplicates_removed == 1); // Same.ttf
  CHECK(result.conflicts_kept == 1);     // mine.js
  CHECK(!result.completed);
  CHECK(read(current + QStringLiteral("/user-fonts/Dropped.ttf")) == "font-bytes");
  CHECK(read(current + QStringLiteral("/user-fonts/Newer.ttf")) == "newer");
  CHECK(read(current + QStringLiteral("/scripts/Demos/mine.js")) == "current edit");
  CHECK(QFileInfo::exists(current + QStringLiteral("/scripts/Empty/.keep")));
  CHECK(!QFileInfo::exists(legacy + QStringLiteral("/user-fonts/Dropped.ttf")));
  CHECK(!QFileInfo::exists(legacy + QStringLiteral("/user-fonts/Same.ttf")));
  CHECK(!QDir(legacy + QStringLiteral("/user-fonts")).exists());
  CHECK(!QDir(legacy + QStringLiteral("/scripts/Empty")).exists());
  CHECK(!QDir(legacy + QStringLiteral("/scripts/Nothing")).exists());
  CHECK(read(legacy + QStringLiteral("/scripts/Demos/mine.js")) == "legacy edit");

  // A second run is a no-op for the conflict, and once the user resolves it the legacy
  // tree disappears entirely, organization folder included.
  result = migration::migrate_app_data_directory(legacy, current);
  CHECK(result.legacy_found && result.conflicts_kept == 1 && result.files_moved == 0);
  CHECK(QFile::remove(legacy + QStringLiteral("/scripts/Demos/mine.js")));
  result = migration::migrate_app_data_directory(legacy, current);
  CHECK(result.legacy_found && result.completed);
  CHECK(!QDir(root + QStringLiteral("/Seth A. Robinson")).exists());
  CHECK(read(current + QStringLiteral("/scripts/Demos/mine.js")) == "current edit");

  // The running app resolves the legacy folder from the legacy organization name and
  // restores the current name afterwards.
  const auto organization = QCoreApplication::organizationName();
  QCoreApplication::setOrganizationName(QStringLiteral("RTsoft"));
  const auto legacy_dir = migration::legacy_app_data_directory();
  CHECK(legacy_dir.contains(QStringLiteral("Seth A. Robinson")));
  CHECK(QCoreApplication::organizationName() == QStringLiteral("RTsoft"));
  QCoreApplication::setOrganizationName(organization);
}

void ui_about_dialog_shows_memory_row() {
  bool inspected = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchySplashScreen"));
    CHECK(dialog != nullptr);

    // The row hides itself on platforms with no memory probe; where a probe
    // exists the value is live, so only the prefix is stable.
    auto* memory = dialog->findChild<QLabel*>(QStringLiteral("splashMemory"));
    CHECK(memory != nullptr);
    CHECK(memory->isHidden() || memory->text().startsWith(QStringLiteral("Memory used")));

    // The memory row must not join the splashCredit pair the links test pins.
    CHECK(dialog->findChildren<QLabel*>(QStringLiteral("splashCredit")).size() == 2);

    inspected = true;
    dialog->accept();
  });

  patchy::ui::show_about_splash();
  CHECK(inspected);
}

void ui_frameless_window_edges_resize() {
  if (!patchy::ui::MainWindow::use_custom_window_chrome()) {
    // macOS/Linux use the native frame; the OS owns the resize borders and the Qt-level
    // edge machinery under test here is deliberately inert.
    std::cout << "[SKIP] native window frame owns resize borders on this platform\n";
    return;
  }
  patchy::ui::MainWindow window;
  show_window(window);
  window.resize(980, 720);
  QApplication::processEvents();

#ifdef Q_OS_WIN
  if (QGuiApplication::platformName() == QStringLiteral("windows")) {
    const auto style = GetWindowLongPtrW(reinterpret_cast<HWND>(window.winId()), GWL_STYLE);
    CHECK((style & WS_THICKFRAME) != 0);
    CHECK((style & WS_CAPTION) == 0);
  }
#endif

  const auto start = window.geometry();
  const QPoint right_edge(window.width() - 2, window.height() / 2);
  send_mouse(window, QEvent::MouseButtonPress, right_edge, Qt::LeftButton, Qt::LeftButton);
  send_mouse(window, QEvent::MouseMove, right_edge + QPoint(90, 0), Qt::NoButton, Qt::LeftButton);
  send_mouse(window, QEvent::MouseButtonRelease, right_edge + QPoint(90, 0), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(window.width() >= start.width() + 70);
  CHECK(window.height() == start.height());

  const auto widened = window.geometry();
  const QPoint bottom_right(window.width() - 2, window.height() - 2);
  send_mouse(window, QEvent::MouseButtonPress, bottom_right, Qt::LeftButton, Qt::LeftButton);
  send_mouse(window, QEvent::MouseMove, bottom_right + QPoint(45, 55), Qt::NoButton, Qt::LeftButton);
  send_mouse(window, QEvent::MouseButtonRelease, bottom_right + QPoint(45, 55), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(window.width() >= widened.width() + 30);
  CHECK(window.height() >= widened.height() + 40);

  const auto expanded = window.geometry();
  const QPoint left_edge(2, window.height() / 2);
  send_mouse(window, QEvent::MouseButtonPress, left_edge, Qt::LeftButton, Qt::LeftButton);
  send_mouse(window, QEvent::MouseMove, left_edge - QPoint(60, 0), Qt::NoButton, Qt::LeftButton);
  send_mouse(window, QEvent::MouseButtonRelease, left_edge - QPoint(60, 0), Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();
  CHECK(window.x() <= expanded.x() - 45);
  CHECK(window.width() >= expanded.width() + 45);
}

void ui_maximized_title_bar_click_keeps_window_maximized() {
  if (!patchy::ui::MainWindow::use_custom_window_chrome()) {
    std::cout << "[SKIP] native window frame owns the title bar on this platform\n";
    return;
  }
  // The Qt-level title-bar fallback (the real Windows window answers HTCAPTION instead, see
  // MainWindow::nativeEvent) must follow the native contract: a click or a sub-threshold
  // wiggle leaves a maximized window maximized, a real drag restores it, and a double-click
  // toggles the state.
  patchy::ui::MainWindow window;
  show_window(window);
  window.showMaximized();
  QApplication::processEvents();
  CHECK(window.isMaximized());

  auto* bar = window.menuBar();
  CHECK(bar != nullptr);
  CHECK(!bar->actions().isEmpty());
  auto* controls = window.findChild<QWidget*>(QStringLiteral("windowChromeControls"));
  CHECK(controls != nullptr);
  // The offscreen screen is small, so a maximized menu bar may have titles running up to
  // the window controls; find a real blank spot (the badge gutter before the first title
  // also counts, it is transparent for mouse events) rather than assuming one past the
  // last title.
  QPoint blank(-1, bar->height() / 2);
  for (int x = controls->x() - 1; x >= 0; --x) {
    if (bar->actionAt(QPoint(x, blank.y())) == nullptr) {
      blank.setX(x);
      break;
    }
  }
  CHECK(blank.x() >= 0);
  CHECK(bar->actionAt(blank) == nullptr);

  send_mouse(*bar, QEvent::MouseButtonPress, blank, Qt::LeftButton, Qt::LeftButton);
  send_mouse(*bar, QEvent::MouseButtonRelease, blank, Qt::LeftButton, Qt::NoButton);
  CHECK(window.isMaximized());

  const QPoint wiggle = blank + QPoint(2, 1);
  send_mouse(*bar, QEvent::MouseButtonPress, blank, Qt::LeftButton, Qt::LeftButton);
  send_mouse(*bar, QEvent::MouseMove, wiggle, Qt::NoButton, Qt::LeftButton);
  send_mouse(*bar, QEvent::MouseButtonRelease, wiggle, Qt::LeftButton, Qt::NoButton);
  CHECK(window.isMaximized());

  const QPoint dragged = blank + QPoint(QApplication::startDragDistance() * 4, 30);
  send_mouse(*bar, QEvent::MouseButtonPress, blank, Qt::LeftButton, Qt::LeftButton);
  send_mouse(*bar, QEvent::MouseMove, dragged, Qt::NoButton, Qt::LeftButton);
  CHECK(!window.isMaximized());
  send_mouse(*bar, QEvent::MouseButtonRelease, dragged, Qt::LeftButton, Qt::NoButton);
  CHECK(!window.isMaximized());

  const auto double_click = [&] {
    send_mouse(*bar, QEvent::MouseButtonPress, blank, Qt::LeftButton, Qt::LeftButton);
    send_mouse(*bar, QEvent::MouseButtonRelease, blank, Qt::LeftButton, Qt::NoButton);
    send_mouse(*bar, QEvent::MouseButtonDblClick, blank, Qt::LeftButton, Qt::LeftButton);
    send_mouse(*bar, QEvent::MouseButtonRelease, blank, Qt::LeftButton, Qt::NoButton);
  };
  double_click();
  CHECK(window.isMaximized());
  double_click();
  CHECK(!window.isMaximized());

  // Menu titles and the window controls are not title bar: a maximized window keeps its
  // state when they are pressed through the same path.
  window.showMaximized();
  QApplication::processEvents();
  CHECK(window.isMaximized());
  const QPoint on_controls = controls->pos() + QPoint(controls->width() / 2, controls->height() / 2);
  CHECK(!patchy::ui::MainWindowTestAccess::title_bar_drag_area_contains(window, on_controls));
  CHECK(!patchy::ui::MainWindowTestAccess::title_bar_drag_area_contains(
      window, bar->actionGeometry(bar->actions().constFirst()).center()));
  CHECK(patchy::ui::MainWindowTestAccess::title_bar_drag_area_contains(window, blank));
}

void ui_right_edge_scrollbars_remain_draggable() {
  patchy::Document document(64, 64, patchy::PixelFormat::rgb8());
  for (int index = 0; index < 48; ++index) {
    document.add_pixel_layer("Scrollable Layer " + std::to_string(index + 1),
                             solid_pixels(64, 64, patchy::PixelFormat::rgba8(),
                                          QColor(40 + index * 3 % 180, 80, 220, 255)));
  }

  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(std::move(document), QStringLiteral("Scrollbar Edge"));
  QApplication::processEvents();

  auto* layer_list = window.findChild<QListWidget*>(QStringLiteral("layerList"));
  CHECK(layer_list != nullptr);
  layer_list->setFixedHeight(180);
  QApplication::processEvents();

  auto* scroll = layer_list->verticalScrollBar();
  CHECK(scroll != nullptr);
  CHECK(scroll->isVisible());
  CHECK(scroll->maximum() > 0);
  scroll->setValue(scroll->maximum() / 3);
  QApplication::processEvents();

  QStyleOptionSlider option;
  option.initFrom(scroll);
  option.orientation = scroll->orientation();
  option.minimum = scroll->minimum();
  option.maximum = scroll->maximum();
  option.singleStep = scroll->singleStep();
  option.pageStep = scroll->pageStep();
  option.sliderPosition = scroll->sliderPosition();
  option.sliderValue = scroll->value();
  option.upsideDown = scroll->invertedAppearance();
  const auto handle = scroll->style()->subControlRect(QStyle::CC_ScrollBar, &option,
                                                      QStyle::SC_ScrollBarSlider, scroll);
  CHECK(handle.isValid());
  const auto start_x = std::clamp(scroll->width() - 2, handle.left(), handle.right());
  const QPoint start(start_x, handle.center().y());
  CHECK(handle.contains(start));
  CHECK(scroll->mapTo(&window, start).x() >= window.width() - 10);

  const auto geometry_before = window.geometry();
  const auto scroll_before = scroll->value();
  const auto end = start + QPoint(0, 55);
  send_mouse(*scroll, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
  send_mouse(*scroll, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
  send_mouse(*scroll, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();

  CHECK(window.geometry() == geometry_before);
  CHECK(scroll->value() > scroll_before);
}

void ui_startup_opens_empty_workspace_with_start_panel() {
  {
    auto settings = patchy::ui::app_settings();
    settings.remove(QStringLiteral("newDocument"));
  }
  patchy::ui::MainWindow window;
  show_window_empty(window);

  auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget());
  auto* panel = window.findChild<QWidget*>(QStringLiteral("startPanel"));
  auto* new_button = window.findChild<QPushButton*>(QStringLiteral("startPanelNewButton"));
  auto* open_button = window.findChild<QPushButton*>(QStringLiteral("startPanelOpenButton"));
  auto* info = window.findChild<QLabel*>(QStringLiteral("documentInfoLabel"));
  CHECK(tabs != nullptr);
  CHECK(panel != nullptr);
  CHECK(new_button != nullptr);
  CHECK(open_button != nullptr);
  CHECK(info != nullptr);

  // Startup: no auto-created document, the start panel fills the tab area.
  CHECK(tabs->count() == 0);
  CHECK(patchy::ui::MainWindowTestAccess::session_count(window) == 0);
  CHECK(panel->isVisible());
  CHECK(panel->size() == tabs->size());
  CHECK(info->text() == QStringLiteral("No document"));
  CHECK(require_action(window, "fileNewAction")->isEnabled());
  CHECK(require_action(window, "fileOpenAction")->isEnabled());
  CHECK(!require_action(window, "fileSaveAction")->isEnabled());
  CHECK(!require_action(window, "filePlaceEmbeddedAction")->isEnabled());
  save_widget_artifact("ui_start_panel", window);

  // The New Document button runs the regular dialog flow and the panel hides.
  accept_new_document_dialog(400, 300);
  new_button->click();
  QApplication::processEvents();
  CHECK(tabs->count() == 1);
  CHECK(!panel->isVisible());
  CHECK(info->text().contains(QStringLiteral("400 x 300 px")));

  // Closing the last document brings the panel back.
  CHECK(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Qt::DirectConnection, Q_ARG(int, 0)));
  QApplication::processEvents();
  CHECK(tabs->count() == 0);
  CHECK(panel->isVisible());
}

void ui_start_panel_recent_files_open_on_click() {
  ensure_artifact_dir();
  const auto recent_path = QFileInfo(QStringLiteral("test-artifacts/start_panel_recent.png")).absoluteFilePath();
  {
    QImage recent_image(48, 32, QImage::Format_RGB32);
    recent_image.fill(QColor(90, 150, 210));
    CHECK(recent_image.save(recent_path));
    auto settings = patchy::ui::app_settings();
    // A dead entry ahead of the live one: the panel list filters to existing files.
    settings.setValue(QStringLiteral("recentFiles"),
                      QStringList{QStringLiteral("Z:/definitely/missing/file.png"), recent_path});
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window_empty(window);
  auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget());
  auto* panel = window.findChild<QWidget*>(QStringLiteral("startPanel"));
  auto* recent_list = window.findChild<QListWidget*>(QStringLiteral("startPanelRecentList"));
  auto* info = window.findChild<QLabel*>(QStringLiteral("documentInfoLabel"));
  CHECK(tabs != nullptr);
  CHECK(panel != nullptr);
  CHECK(recent_list != nullptr);
  CHECK(info != nullptr);
  CHECK(panel->isVisible());
  CHECK(recent_list->isVisible());
  // The dead entry drops once the background existence check reports back.
  CHECK(process_events_until([&] { return recent_list->count() == 1; }));
  CHECK(recent_list->item(0)->text() == QStringLiteral("start_panel_recent.png"));

  auto* layers = window.findChild<QListWidget*>(QStringLiteral("layerList"));
  CHECK(layers != nullptr);
  int layer_rebuilds = 0;
  QObject::connect(layers->model(), &QAbstractItemModel::modelReset, &window, [&] { ++layer_rebuilds; });

  const auto row_center = recent_list->visualItemRect(recent_list->item(0)).center();
  send_mouse(*recent_list->viewport(), QEvent::MouseButtonPress, row_center, Qt::LeftButton, Qt::LeftButton);
  send_mouse(*recent_list->viewport(), QEvent::MouseButtonRelease, row_center, Qt::LeftButton, Qt::NoButton);
  QApplication::processEvents();

  CHECK(tabs->count() == 1);
  CHECK(!panel->isVisible());
  CHECK(info->text().contains(QStringLiteral("48 x 32 px")));
  CHECK(layer_rebuilds == 1);
}

void ui_large_document_session_keeps_loading_responsive() {
  patchy::ui::MainWindow window;
  show_window_empty(window);
  auto* panel = window.findChild<QWidget*>(QStringLiteral("startPanel"));
  auto* layers = window.findChild<QListWidget*>(QStringLiteral("layerList"));
  CHECK(layers != nullptr);
  CHECK(panel != nullptr && panel->isVisible());

  patchy::Document document(96, 96, patchy::PixelFormat::rgba8());
  constexpr int row_count = 2048;
  for (int index = 0; index < row_count; ++index) {
    document.add_layer(patchy::Layer(document.allocate_layer_id(), "Layer " + std::to_string(index),
                                    solid_pixels(1, 1, patchy::PixelFormat::rgba8(), QColor(80, 140, 210))));
  }
  EnvironmentVariableRestorer restore_delay("PATCHY_PROCESSING_RENDER_TEST_DELAY_MS");
  qputenv("PATCHY_PROCESSING_RENDER_TEST_DELAY_MS", QByteArray("2000"));

  int rebuilds = 0;
  int partial_row_ticks = 0;
  bool welcome_over_canvas = false;
  bool canvas_unlocked = false;
  QImage first_spinner;
  QImage changed_spinner;
  QImage loading_window;
  QObject::connect(layers->model(), &QAbstractItemModel::modelReset, &window, [&] { ++rebuilds; });
  QTimer heartbeat;
  heartbeat.setInterval(20);
  QObject::connect(&heartbeat, &QTimer::timeout, &window, [&] {
    auto* canvas = window.findChild<patchy::ui::CanvasWidget*>();
    if (canvas == nullptr || layers->count() != row_count ||
        layers->itemWidget(layers->item(0)) == nullptr ||
        layers->itemWidget(layers->item(row_count - 1)) != nullptr) {
      return;
    }
    ++partial_row_ticks;
    welcome_over_canvas = welcome_over_canvas || panel->isVisible();
    canvas_unlocked = canvas_unlocked || !canvas->edit_locked();
    if (!canvas->render_settled() && changed_spinner.isNull()) {
      const auto badge = canvas->grab().toImage().copy((canvas->width() - 168) / 2, 12, 168, 50);
      if (first_spinner.isNull()) {
        first_spinner = badge;
      } else if (!images_equal_rgba(first_spinner, badge)) {
        changed_spinner = badge;
        loading_window = window.grab().toImage();
      }
    }
  });
  heartbeat.start();
  window.add_document_session(std::move(document), QStringLiteral("Many layers"));
  heartbeat.stop();

  auto* canvas = window.findChild<patchy::ui::CanvasWidget*>();
  CHECK(canvas != nullptr);
  CHECK(rebuilds == 1);
  CHECK(partial_row_ticks >= 2);
  CHECK(!welcome_over_canvas);
  CHECK(!canvas_unlocked);
  CHECK(!first_spinner.isNull() && !changed_spinner.isNull());
  ensure_artifact_dir();
  CHECK(loading_window.save(QStringLiteral("test-artifacts/ui_large_document_session_loading.png")));
  CHECK(!canvas->edit_locked());
  CHECK(!panel->isVisible());
  CHECK(layers->count() == row_count);
  for (int row = 0; row < row_count; ++row) {
    CHECK(layers->itemWidget(layers->item(row)) != nullptr);
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (!canvas->render_settled() && std::chrono::steady_clock::now() < deadline) {
    QApplication::processEvents();
    QThread::msleep(5);
  }
  CHECK(canvas->render_settled());
  save_widget_artifact("ui_large_document_session_loaded", window);
}

void ui_start_panel_recent_files_scroll_and_context_menu() {
  ensure_artifact_dir();
  constexpr int kSeededFiles = 14;  // more than the eight rows the box shows at once
  QStringList recent_files;
  for (int index = 0; index < kSeededFiles; ++index) {
    const auto path =
        QFileInfo(QStringLiteral("test-artifacts/start_panel_scroll_%1.png").arg(index, 2, 10, QLatin1Char('0')))
            .absoluteFilePath();
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(QColor(40 + index * 8, 120, 200));
    CHECK(image.save(path));
    recent_files << path;
  }
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), recent_files);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window_empty(window);
  auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget());
  auto* recent_list = window.findChild<QListWidget*>(QStringLiteral("startPanelRecentList"));
  CHECK(tabs != nullptr);
  CHECK(recent_list != nullptr);
  CHECK(recent_list->isVisible());

  // A window with no room for eight rows shrinks the box and still scrolls, rather
  // than pushing the panel footer off screen.
  CHECK(recent_list->count() == kSeededFiles);
  CHECK(recent_list->height() < 8 * 40 + 14);
  CHECK(recent_list->height() >= 4 * 40);
  CHECK(recent_list->verticalScrollBar()->maximum() > 0);

  // Tall enough for the whole panel, so the box reaches its eight-row cap instead.
  window.resize(1180, 1000);
  QApplication::processEvents();

  // Every remembered file is a row, but the box stops at eight and scrolls for
  // the rest. Before this cap the list took QAbstractScrollArea's default hint
  // and silently clipped everything past the fourth row.
  CHECK(recent_list->count() == kSeededFiles);
  CHECK(recent_list->sizeHint().height() == 8 * 40 + 14);
  CHECK(recent_list->height() == 8 * 40 + 14);
  CHECK(recent_list->verticalScrollBar()->maximum() > 0);
  CHECK(recent_list->verticalScrollBar()->isVisible());
  CHECK(recent_list->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff);
  save_widget_artifact("ui_start_panel_recent_scroll", window);

  recent_list->verticalScrollBar()->setValue(recent_list->verticalScrollBar()->maximum());
  QApplication::processEvents();
  CHECK(recent_list->visualItemRect(recent_list->item(kSeededFiles - 1)).intersects(recent_list->viewport()->rect()));

  // Right-click a row: the start panel gets the same menu the Open Recent menu builds.
  QApplication::clipboard()->clear();
  bool saw_context_menu = false;
  QTimer::singleShot(0, [&] {
    for (auto* widget : QApplication::topLevelWidgets()) {
      auto* menu = qobject_cast<QMenu*>(widget);
      if (menu == nullptr || menu->objectName() != QStringLiteral("recentFileContextMenu")) {
        continue;
      }
      auto* copy_action = find_menu_action_by_text(*menu, QStringLiteral("Copy File Path"));
      auto* explorer_action = find_menu_action_by_text(*menu, QStringLiteral("Open in File Explorer"));
      CHECK(copy_action != nullptr);
      CHECK(explorer_action != nullptr);
      CHECK(copy_action->objectName() == QStringLiteral("recentFileCopyPathAction"));
      CHECK(explorer_action->objectName() == QStringLiteral("recentFileOpenInExplorerAction"));
      copy_action->trigger();
      menu->close();
      saw_context_menu = true;
      return;
    }
    CHECK(false);
  });

  auto* target_item = recent_list->item(kSeededFiles - 1);
  const auto context_point = recent_list->visualItemRect(target_item).center();
  QContextMenuEvent context_event(QContextMenuEvent::Mouse, context_point,
                                  recent_list->viewport()->mapToGlobal(context_point));
  QApplication::sendEvent(recent_list->viewport(), &context_event);
  QApplication::processEvents();

  CHECK(saw_context_menu);
  CHECK(QApplication::clipboard()->text() == QDir::toNativeSeparators(recent_files.last()));
  CHECK(window.statusBar()->currentMessage() == QStringLiteral("File path copied"));
  // A right-click must never also open the row the way a left-click does.
  CHECK(tabs->count() == 0);
}

void ui_start_panel_recent_filter_narrows_rows_and_opens_match() {
  ensure_artifact_dir();
  // An empty kRecentPathRole path marks the no-match placeholder (ui/recent_file_list.hpp).
  using patchy::ui::kRecentPathRole;
  QStringList recent_files;
  for (const auto* name : {"start_panel_filter_alpha", "start_panel_filter_beta", "start_panel_filter_gamma"}) {
    const auto path =
        QFileInfo(QStringLiteral("test-artifacts/%1.png").arg(QLatin1String(name))).absoluteFilePath();
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(QColor(70, 130, 180));
    CHECK(image.save(path));
    recent_files << path;
  }

  SettingsValueRestorer recent_files_restorer(QStringLiteral("recentFiles"));
  {
    auto settings = patchy::ui::app_settings();
    settings.setValue(QStringLiteral("recentFiles"), recent_files);
    settings.sync();
  }

  patchy::ui::MainWindow window;
  show_window_empty(window);
  auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget());
  auto* panel = window.findChild<QWidget*>(QStringLiteral("startPanel"));
  auto* recent_list = window.findChild<QListWidget*>(QStringLiteral("startPanelRecentList"));
  auto* filter_edit = window.findChild<QLineEdit*>(QStringLiteral("startPanelRecentFilterEdit"));
  CHECK(tabs != nullptr);
  CHECK(panel != nullptr);
  CHECK(recent_list != nullptr);
  CHECK(filter_edit != nullptr);
  CHECK(filter_edit->isVisible());
  CHECK(filter_edit->text().isEmpty());
  CHECK(recent_list->count() == 3);
  // The panel opens with the filter focused, so a name can be typed straight away.
  CHECK(window.focusWidget() == filter_edit);

  filter_edit->setText(QStringLiteral("beta"));
  QApplication::processEvents();
  CHECK(recent_list->count() == 1);
  CHECK(recent_list->item(0)->text() == QStringLiteral("start_panel_filter_beta.png"));

  // The clear button sits centered inside the field. The application QToolButton
  // rule's 26 px minimum used to be applied to it as a real minimum size, which
  // pushed the x below the 24 px edit's bottom border.
  // Qt names the clear action, not the button, so this finds it by type.
  auto* clear_button = filter_edit->findChild<QToolButton*>();
  CHECK(clear_button != nullptr);
  CHECK(clear_button->isVisible());
  CHECK(clear_button->height() <= filter_edit->height());
  CHECK(clear_button->geometry().right() < filter_edit->width());
  CHECK(std::abs(clear_button->geometry().center().y() - filter_edit->rect().center().y()) <= 1);

  // The whole path is matched, so a folder name narrows the same way a file name does.
  filter_edit->setText(QStringLiteral("test-artifacts"));
  QApplication::processEvents();
  CHECK(recent_list->count() == 3);

  // Nothing matching keeps the box in place with one dimmed placeholder row that
  // opens no document when clicked.
  filter_edit->setText(QStringLiteral("no-such-recent-file"));
  QApplication::processEvents();
  CHECK(recent_list->isVisible());
  CHECK(recent_list->count() == 1);
  CHECK(recent_list->item(0)->data(kRecentPathRole).toString().isEmpty());
  // The clear button fades in on a property animation, so the artifact needs the
  // event loop to run before it shows the x in its final place.
  process_events_for(250);
  save_widget_artifact("ui_start_panel_recent_filter", window);
  const auto placeholder_center = recent_list->visualItemRect(recent_list->item(0)).center();
  send_mouse(*recent_list->viewport(), QEvent::MouseButtonPress, placeholder_center, Qt::LeftButton,
             Qt::LeftButton);
  send_mouse(*recent_list->viewport(), QEvent::MouseButtonRelease, placeholder_center, Qt::LeftButton,
             Qt::NoButton);
  QApplication::processEvents();
  CHECK(tabs->count() == 0);
  CHECK(panel->isVisible());

  // Enter opens the top match without touching the mouse.
  filter_edit->setText(QStringLiteral("gamma"));
  QApplication::processEvents();
  CHECK(recent_list->count() == 1);
  send_key(*filter_edit, Qt::Key_Return);
  QApplication::processEvents();
  CHECK(tabs->count() == 1);
  CHECK(!panel->isVisible());
  CHECK(tabs->tabText(0) == QStringLiteral("start_panel_filter_gamma.png"));

  // The filter starts clean every time the panel comes back.
  CHECK(QMetaObject::invokeMethod(tabs, "tabCloseRequested", Qt::DirectConnection, Q_ARG(int, 0)));
  QApplication::processEvents();
  CHECK(panel->isVisible());
  CHECK(filter_edit->text().isEmpty());
  CHECK(recent_list->count() == 3);
}

void ui_start_panel_shows_about_info_and_update_status() {
  patchy::ui::MainWindow window;
  show_window_empty(window);

  auto* panel = window.findChild<patchy::ui::StartPanel*>(QStringLiteral("startPanel"));
  CHECK(panel != nullptr);
  CHECK(panel->isVisible());

  // The about branding lives on the panel now: tagline, version, credit, and links.
  auto* tagline = window.findChild<QLabel*>(QStringLiteral("startPanelTagline"));
  CHECK(tagline != nullptr);
  CHECK(tagline->text() == QStringLiteral("Open source photo editing. Free forever, no subscriptions."));
  auto* version = window.findChild<QLabel*>(QStringLiteral("startPanelVersion"));
  CHECK(version != nullptr);
  // The number comes from the configure-time generated patchy_version.hpp
  // (cmake/patchy_version.hpp.in), so the label must show the configured
  // version verbatim: an empty or placeholder value means the header broke.
  CHECK(version->text() == QStringLiteral("Version %1 (built %2)")
                               .arg(QStringLiteral(PATCHY_VERSION), patchy::ui::build_timestamp_text()));
  CHECK(!QStringLiteral(PATCHY_VERSION).isEmpty());
  CHECK(QStringLiteral(PATCHY_VERSION) != QStringLiteral("0.0.0"));
  auto* credit = window.findChild<QLabel*>(QStringLiteral("startPanelCredit"));
  CHECK(credit != nullptr);
  CHECK(credit->textFormat() == Qt::RichText);
  CHECK(credit->openExternalLinks());
  CHECK(credit->text().startsWith(QStringLiteral("Created by ")));
  CHECK(credit->text().contains(QStringLiteral("href=\"https://github.com/SethRobinson\"")));
  CHECK(credit->text().contains(QStringLiteral(">Seth A. Robinson</a>")));
  CHECK(!credit->text().contains(QStringLiteral("@link_text")));
  // The contributor credits belong to the About dialog alone; the start panel
  // keeps its footer short so the recent-files list gets the room.
  CHECK(window.findChild<QLabel*>(QStringLiteral("startPanelContributors")) == nullptr);

  const auto link_labels = panel->findChildren<QLabel*>(QStringLiteral("startPanelHome"));
  CHECK(link_labels.size() == 2);
  QString combined_text;
  for (const auto* label : link_labels) {
    CHECK(label->textFormat() == Qt::RichText);
    CHECK(label->textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse));
    CHECK(label->openExternalLinks());
    combined_text += label->text();
    combined_text += QLatin1Char('\n');
  }
  CHECK(combined_text.contains(QStringLiteral("href=\"https://github.com/SethRobinson/Patchy\"")));
  CHECK(combined_text.contains(QStringLiteral(">SethRobinson/Patchy</a>")));
  CHECK(combined_text.contains(QStringLiteral("href=\"https://rtsoft.com\"")));
  CHECK(combined_text.contains(QStringLiteral(">rtsoft.com</a>")));

  // The update-status footer line stays hidden until a status is pushed and
  // hides again when cleared.
  auto* status = window.findChild<QLabel*>(QStringLiteral("startPanelUpdateStatus"));
  CHECK(status != nullptr);
  CHECK(!status->isVisible());
  panel->set_update_status(QStringLiteral("Update available: Patchy 9.99."));
  QApplication::processEvents();
  CHECK(status->isVisible());
  CHECK(status->text() == QStringLiteral("Update available: Patchy 9.99."));
  save_widget_artifact("ui_start_panel_about_info", window);
  panel->set_update_status(QString());
  CHECK(!status->isVisible());
}

void ui_status_bar_error_message_flashes_then_persists_until_replaced() {
  patchy::ui::ZoomStatusBar bar;
  bar.resize(600, 24);
  bar.show();
  QApplication::processEvents();

  bar.showMessage(QStringLiteral("Ready"));
  CHECK(!bar.error_message_active());

  bar.show_error_message(QStringLiteral("Layer pixels are locked."));
  CHECK(bar.currentMessage() == QStringLiteral("Layer pixels are locked."));
  CHECK(bar.error_message_active());
  CHECK(bar.error_flash_running());

  // The flash is finite (~1s); the red error presentation persists after it.
  CHECK(process_events_until([&bar] { return !bar.error_flash_running(); }));
  CHECK(bar.error_message_active());

  // Re-showing the SAME error restarts the flash even though QStatusBar
  // suppresses messageChanged for an identical string.
  bar.show_error_message(QStringLiteral("Layer pixels are locked."));
  CHECK(bar.error_flash_running());
  CHECK(bar.currentMessage() == QStringLiteral("Layer pixels are locked."));

  // Any different message ends the error presentation.
  bar.showMessage(QStringLiteral("Applied Levels"));
  CHECK(!bar.error_message_active());
  CHECK(!bar.error_flash_running());

  bar.show_error_message(QStringLiteral("Layer pixels are locked."));
  CHECK(bar.error_message_active());
  bar.clearMessage();
  CHECK(!bar.error_message_active());
}

void ui_blocking_refusal_shows_error_status_and_info_clears_it() {
  patchy::ui::MainWindow window;
  show_window(window);

  require_action(window, "filterConvertForSmartFiltersAction")->trigger();
  QApplication::processEvents();
  require_action(window, "filterLiquifyAction")->trigger();
  QApplication::processEvents();

  CHECK(window.statusBar()->currentMessage() ==
        QStringLiteral("Rasterize the Smart Object before using Liquify"));
  auto* bar = qobject_cast<patchy::ui::ZoomStatusBar*>(window.statusBar());
  CHECK(bar != nullptr);
  CHECK(bar->error_message_active());
  save_widget_artifact("status_bar_error_flash", *bar);

  window.statusBar()->showMessage(QStringLiteral("Ready"));
  QApplication::processEvents();
  CHECK(!bar->error_message_active());
}

// A relaunch hands its foreground right to the running instance at the far end of the
// single-instance pipe, so it must be able to name that process.
void ui_single_instance_socket_names_the_server_process() {
  QLocalServer server;
  const auto name = QStringLiteral("Patchy-SingleInstanceTest-%1").arg(QCoreApplication::applicationPid());
  QLocalServer::removeServer(name);
  CHECK(server.listen(name));
  QLocalSocket client;
  client.connectToServer(name);
  CHECK(client.waitForConnected(2000));
  const auto process_id = patchy::ui::local_socket_server_process_id(client);
#ifdef Q_OS_WIN
  CHECK(process_id.has_value() && *process_id == QCoreApplication::applicationPid());
#else
  CHECK(!process_id.has_value());
  CHECK(!patchy::ui::allow_local_socket_server_to_take_foreground(client));
#endif
  client.disconnectFromServer();
  QLocalSocket unconnected;
  CHECK(!patchy::ui::local_socket_server_process_id(unconnected).has_value());
}

// A relaunch restores a minimized window, and with a modal dialog open it is the dialog that
// ends up active: file opens wait for the dialog, the foreground must not.
void ui_second_instance_brings_window_and_modal_dialog_forward() {
  patchy::ui::MainWindow window;
  show_window(window);
  window.showMinimized();
  QApplication::processEvents();
  CHECK(window.isMinimized());
  window.bring_to_front_for_second_instance();
  QApplication::processEvents();
  CHECK(!window.isMinimized());
  CHECK(window.isVisible());

  QDialog dialog(&window);
  dialog.setObjectName(QStringLiteral("secondInstanceTestDialog"));
  dialog.setModal(true);
  dialog.show();
  QApplication::processEvents();
  CHECK(QApplication::activeModalWidget() == &dialog);
  // Offscreen lets the blocked window take activation, which is the state a relaunch must undo.
  window.activateWindow();
  CHECK(QTest::qWaitFor([&window] { return QApplication::activeWindow() == &window; }, 2000));
  window.bring_to_front_for_second_instance();
  CHECK(QTest::qWaitFor([&dialog] { return QApplication::activeWindow() == &dialog; }, 2000));
  dialog.reject();
}

void ui_svg_icon_resources_are_registered() {
  patchy::ui::MainWindow window;
  show_window(window);

  CHECK(QFile::exists(QStringLiteral(":/patchy/icons/new.svg")));
  CHECK(QFile::exists(QStringLiteral(":/patchy/icons/mask.svg")));
  CHECK(QFile::exists(QStringLiteral(":/patchy/icons/selection-add.svg")));
  CHECK(QFile::exists(QStringLiteral(":/patchy/icons/channel-save-selection.svg")));
  CHECK(QFile::exists(QStringLiteral(":/patchy/icons/channel-load-selection.svg")));

  const QIcon icon(QStringLiteral(":/patchy/icons/new.svg"));
  CHECK(!icon.isNull());
  CHECK(!icon.pixmap(QSize(32, 32)).isNull());

  CHECK(!require_action(window, "layerNewAction")->icon().isNull());
  CHECK(!require_action(window, "layerAddMaskAction")->icon().isNull());
}

// The icon engine recolors a fixed ten-color vocabulary. A new icon drawn with a
// color outside it would render fine in Dark and then stay dark-on-dark in Light,
// which no rendered test would catch. Walk the resources instead.
void ui_icon_color_map_covers_every_authored_color() {
  QSet<QString> mapped;
  for (const auto& [source, member] : patchy::ui::icon_color_roles()) {
    mapped.insert(QString(source).toLower());
  }
  CHECK(!mapped.isEmpty());

  QSet<QString> exempt;
  for (const auto& name : patchy::ui::literal_color_icon_names()) {
    exempt.insert(name);
  }
  for (const auto& name : patchy::ui::stylesheet_referenced_icon_names()) {
    exempt.insert(name);
  }

  const QDir icons(QStringLiteral(":/patchy/icons"));
  const auto entries = icons.entryList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name);
  CHECK(entries.size() > 50);

  static const QRegularExpression hex(QStringLiteral("#[0-9a-fA-F]{6}"));
  int checked = 0;
  for (const auto& entry : entries) {
    QFile file(icons.filePath(entry));
    CHECK(file.open(QIODevice::ReadOnly));
    const QString svg = QString::fromUtf8(file.readAll());
    const QString stem = QFileInfo(entry).completeBaseName();

    auto matches = hex.globalMatch(svg);
    while (matches.hasNext()) {
      const auto value = matches.next().captured(0);
      // The substitution is a plain byte replace, so authored hex must be lowercase.
      if (value != value.toLower()) {
        fprintf(stderr, "  %s: uppercase hex %s\n", qPrintable(entry), qPrintable(value));
      }
      CHECK(value == value.toLower());
      if (exempt.contains(stem)) {
        continue;
      }
      if (!mapped.contains(value)) {
        fprintf(stderr, "  %s: %s is not in the icon color map\n", qPrintable(entry), qPrintable(value));
      }
      CHECK(mapped.contains(value));
      ++checked;
    }
  }
  CHECK(checked > 100);

  // Every stylesheet-referenced icon must actually exist under the base path; the
  // light variants are checked separately once they exist.
  for (const auto& name : patchy::ui::stylesheet_referenced_icon_names()) {
    CHECK(QFile::exists(QStringLiteral(":/patchy/icons/%1.svg").arg(name)));
  }
}

// A role omitted from a palette's aggregate initializer default-constructs to an
// invalid QColor, which QSS then renders as a black fill rather than failing.
// Walking the role table is the only thing that catches it.
void ui_theme_palettes_define_every_role() {
  const auto roles = patchy::ui::theme_palette_roles();
  CHECK(!roles.empty());

  for (const auto& [name, member] : roles) {
    const auto dark = patchy::ui::dark_palette().*member;
    const auto light = patchy::ui::light_palette().*member;
    if (!dark.isValid() || !light.isValid()) {
      fprintf(stderr, "  role \"%s\" is missing a value\n", QString(name).toUtf8().constData());
    }
    CHECK(dark.isValid());
    CHECK(light.isValid());
  }

  // Role names are generated from the member spelling, so duplicates mean a
  // copy-paste in the table rather than a typo.
  QSet<QString> seen;
  for (const auto& [name, member] : roles) {
    CHECK(!seen.contains(QString(name)));
    seen.insert(QString(name));
  }
}

// Handing token text to setStyleSheet is silently destructive: Qt drops the whole
// declaration, so a rule simply stops existing and only shows up as a wrongly
// drawn control in one state. ThemedQss makes that a compile error for the shared
// builders; this catches an inline blob that forgot to go through the helpers.
void ui_no_widget_ships_unresolved_theme_tokens() {
  patchy::ui::MainWindow window;
  show_window(window);
  QApplication::processEvents();

  int styled = 0;
  for (auto* widget : QApplication::allWidgets()) {
    if (widget == nullptr) {
      continue;
    }
    const auto sheet = widget->styleSheet();
    if (sheet.isEmpty()) {
      continue;
    }
    ++styled;
    if (sheet.contains(QLatin1Char('@'))) {
      fprintf(stderr, "  %s (%s) ships an unresolved theme token\n",
              qPrintable(widget->objectName()), widget->metaObject()->className());
    }
    CHECK(!sheet.contains(QLatin1Char('@')));
  }
  CHECK(styled > 0);
}

[[nodiscard]] int channel_delta(const QColor& first, const QColor& second) {
  return std::max({std::abs(first.red() - second.red()), std::abs(first.green() - second.green()),
                   std::abs(first.blue() - second.blue())});
}

// Light is derived from Dark, so two roles that never touch in Dark can land on
// the same value in Light. That happened: title_bar_bg and canvas_backdrop both
// resolved to the same gray and a dialog floating over the canvas lost its title
// bar entirely. Encode the pairs that actually sit against each other on screen.
// Thresholds are set below what Dark already achieves, so this asserts "Light is
// at least as readable here as the scheme we have always shipped".
void ui_theme_adjacent_roles_stay_distinguishable() {
  struct RolePair {
    const char* description;
    QColor patchy::ui::ThemePalette::* first;
    QColor patchy::ui::ThemePalette::* second;
    int minimum_delta;
  };
  const std::array<RolePair, 5> pairs{{
      {"a dialog title bar floating over the canvas", &patchy::ui::ThemePalette::title_bar_bg,
       &patchy::ui::ThemePalette::canvas_backdrop, 24},
      {"the About dialog's secondary button on its body", &patchy::ui::ThemePalette::splash_button_bg,
       &patchy::ui::ThemePalette::splash_bg, 12},
      {"a dialog title bar against the dialog body it heads", &patchy::ui::ThemePalette::title_bar_bg,
       &patchy::ui::ThemePalette::window_bg, 12},
      {"the chrome bar against a docked panel", &patchy::ui::ThemePalette::title_bar_bg,
       &patchy::ui::ThemePalette::panel_bg, 16},
      {"the clipping badge on an unselected layer row", &patchy::ui::ThemePalette::layer_clip_badge,
       &patchy::ui::ThemePalette::layer_row_bg, 60},
  }};

  for (const auto scheme : {patchy::ui::ColorScheme::Dark, patchy::ui::ColorScheme::Light}) {
    const auto& palette = patchy::ui::theme(scheme);
    const char* label = scheme == patchy::ui::ColorScheme::Light ? "light" : "dark";
    for (const auto& pair : pairs) {
      const int measured = channel_delta(palette.*pair.first, palette.*pair.second);
      if (measured < pair.minimum_delta) {
        fprintf(stderr, "  %s: %s -> delta %d, want >= %d\n", label, pair.description, measured,
                pair.minimum_delta);
      }
      CHECK(measured >= pair.minimum_delta);
    }
  }
}

[[nodiscard]] double relative_luminance(const QColor& color) {
  const auto channel = [](double value) {
    return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
  };
  return 0.2126 * channel(color.redF()) + 0.7152 * channel(color.greenF()) +
         0.0722 * channel(color.blueF());
}

[[nodiscard]] double contrast_ratio(const QColor& first, const QColor& second) {
  const double a = relative_luminance(first);
  const double b = relative_luminance(second);
  return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

// Text roles pinned to one value across both schemes are the trap here:
// text_on_accent is correctly white over a saturated selection, and was also
// being used over neutral raised surfaces, where Light turns the background
// pale and the label vanishes. Every text/background pair has to hold up in
// both schemes, not just the one it was authored against.
void ui_theme_text_roles_contrast_with_their_backgrounds() {
  struct TextPair {
    const char* description;
    QColor patchy::ui::ThemePalette::* text;
    QColor patchy::ui::ThemePalette::* background;
  };
  const std::array<TextPair, 11> pairs{{
      {"the About dialog's body text", &patchy::ui::ThemePalette::splash_body_text,
       &patchy::ui::ThemePalette::splash_bg},
      {"a highlighted menu item", &patchy::ui::ThemePalette::text_on_accent,
       &patchy::ui::ThemePalette::menu_item_selected_bg},
      {"a checked options-bar button", &patchy::ui::ThemePalette::text_on_accent,
       &patchy::ui::ThemePalette::accent_checked_bg},
      {"the selected layer-style category", &patchy::ui::ThemePalette::text_on_accent,
       &patchy::ui::ThemePalette::category_selected_bg},
      {"the About dialog's primary button", &patchy::ui::ThemePalette::text_on_accent,
       &patchy::ui::ThemePalette::splash_primary_bg},
      {"the selected document tab", &patchy::ui::ThemePalette::text_on_raised,
       &patchy::ui::ThemePalette::tab_selected_bg},
      {"the selected Preferences tab", &patchy::ui::ThemePalette::text_on_raised,
       &patchy::ui::ThemePalette::dialog_tab_selected_bg},
      {"an Image Size panel label", &patchy::ui::ThemePalette::text_on_raised,
       &patchy::ui::ThemePalette::dlg_raised_bg},
      {"body text on the window surface", &patchy::ui::ThemePalette::text_primary,
       &patchy::ui::ThemePalette::window_bg},
      {"the menu bar", &patchy::ui::ThemePalette::text_bright,
       &patchy::ui::ThemePalette::title_bar_bg},
      {"a selected layer row's name", &patchy::ui::ThemePalette::layer_row_name_text,
       &patchy::ui::ThemePalette::layer_row_selected_bg},
  }};

  // 3:1 is the floor for UI text and large type; every pair here clears it in
  // Dark, so this asserts Light is no worse than what already ships.
  constexpr double kMinimumContrast = 3.0;
  for (const auto scheme : {patchy::ui::ColorScheme::Dark, patchy::ui::ColorScheme::Light}) {
    const auto& palette = patchy::ui::theme(scheme);
    const char* label = scheme == patchy::ui::ColorScheme::Light ? "light" : "dark";
    for (const auto& pair : pairs) {
      const double ratio = contrast_ratio(palette.*pair.text, palette.*pair.background);
      if (ratio < kMinimumContrast) {
        fprintf(stderr, "  %s: %s -> contrast %.2f, want >= %.1f\n", label, pair.description, ratio,
                kMinimumContrast);
      }
      CHECK(ratio >= kMinimumContrast);
    }
  }
}

// The rendered counterpart to the pair above: the palette can be right and the
// chrome still wrong if a rule stops applying. Opens the real dialog over the
// real canvas in Light and looks at the pixels.
void ui_light_scheme_dialog_chrome_separates_from_canvas() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Light);

  patchy::ui::MainWindow window;
  show_window(window);
  QApplication::processEvents();

  bool sampled = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyPreferencesDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* title_bar = dialog->findChild<QWidget*>(QStringLiteral("dialogChromeTitleBar"));
    CHECK(title_bar != nullptr);
    if (title_bar != nullptr) {
      const auto shot = title_bar->grab().toImage();
      CHECK(!shot.isNull());
      if (!shot.isNull()) {
        // Mid-height, right of the badge and left of the close button.
        const auto bar = shot.pixelColor(shot.width() / 2, shot.height() / 2);
        const int against_canvas = channel_delta(bar, patchy::ui::theme().canvas_backdrop);
        const int against_body = channel_delta(bar, patchy::ui::theme().window_bg);
        if (against_canvas < 20 || against_body < 10) {
          fprintf(stderr, "  title bar %s: delta %d vs canvas, %d vs dialog body\n",
                  qPrintable(bar.name(QColor::HexRgb)), against_canvas, against_body);
        }
        CHECK(against_canvas >= 20);
        CHECK(against_body >= 10);
        sampled = true;
      }
    }
    save_widget_artifact("ui_light_scheme_preferences_dialog", *dialog);
    dialog->reject();
  });
  require_action(window, "filePreferencesAction")->trigger();
  QApplication::processEvents();
  CHECK(sampled);
}

// A floating dialog gets no window-manager shadow on wasm, and its title bar
// shares @title_bar_bg with the main window's own chrome bar, so the 1px
// @window_border outline is the only thing that can separate a dialog's edge
// from the chrome behind it. Assert the outline is actually the outermost
// pixel of the rendered dialog and that it reads against both the title-bar
// fill inside it and the chrome surface it typically floats over.
void ui_dialog_chrome_outline_reads_at_the_dialog_edge() {
  ColorSchemeRestorer restore_active;

  for (const auto preference :
       {patchy::ui::ColorSchemePreference::Dark, patchy::ui::ColorSchemePreference::Light}) {
    ColorSchemeRestorer::apply(preference);
    const bool light = preference == patchy::ui::ColorSchemePreference::Light;
    const char* label = light ? "light" : "dark";

    QDialog dialog;
    auto* root = new QVBoxLayout(&dialog);
    auto* content =
        patchy::ui::install_dark_dialog_chrome(dialog, root, QStringLiteral("Outline Probe"));
    content->addWidget(new QLabel(QStringLiteral("Probe body"), &dialog));
    dialog.resize(300, 140);
    dialog.show();
    QApplication::processEvents();

    const auto shot = dialog.grab().toImage();
    CHECK(!shot.isNull());
    if (shot.isNull()) {
      return;
    }
    if (light) {
      save_widget_artifact("ui_light_scheme_dialog_chrome_outline", dialog);
    }

    // Mid-width avoids the badge and the close button; row 0 is the outline
    // if it is drawn at all, and row 17 is inside the 34px title bar's fill.
    const auto edge = shot.pixelColor(shot.width() / 2, 0);
    const auto title_fill = shot.pixelColor(shot.width() / 2, 17);
    const int edge_vs_title = channel_delta(edge, title_fill);
    const int edge_vs_chrome = channel_delta(edge, patchy::ui::theme().title_bar_bg);
    if (edge_vs_title < 40 || edge_vs_chrome < 40) {
      fprintf(stderr, "  %s: dialog edge %s vs title fill %s (%d), vs chrome bar %s (%d), want >= 40\n",
              label, qPrintable(edge.name(QColor::HexRgb)), qPrintable(title_fill.name(QColor::HexRgb)),
              edge_vs_title, qPrintable(patchy::ui::theme().title_bar_bg.name(QColor::HexRgb)),
              edge_vs_chrome);
    }
    CHECK(edge_vs_title >= 40);
    CHECK(edge_vs_chrome >= 40);
  }
}

// Panel and list scroll bars are styled by the application sheet on every
// platform rather than left to the native style. They have to be: the global
// QWidget background rule reaches them anyway, and once QSS touches a scroll bar
// QStyleSheetStyle owns the whole complex control, so one without subcontrol
// rules fills its groove with the window background and lets the base style draw
// the rest on top. Against Dark's near-black surface that passed for native
// rendering, which is how it survived review. In Light the groove, the handle and
// the panel behind them all derived to the same near-white and the bar vanished,
// leaving two faint arrow glyphs as the only evidence it was there.
//
// A role-pair check cannot catch this, because the track is deliberately the same
// value as the surface in both schemes and the separation comes from the handle
// and the dither texture over it. So render a real bar and read it.
void ui_light_scheme_panel_scroll_bar_handle_reads_against_track() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Light);

  patchy::ui::MainWindow window;
  show_window(window);
  QApplication::processEvents();

  // Far more rows than fit, so the handle covers a small fraction of the groove
  // and the bare track below it is actually rendered.
  auto* list = new QListWidget(&window);
  list->setObjectName(QStringLiteral("lightScrollBarProbeList"));
  for (int row = 0; row < 400; ++row) {
    list->addItem(QStringLiteral("row %1").arg(row));
  }
  list->setGeometry(0, 0, 200, 160);
  list->show();
  QApplication::processEvents();

  auto* bar = list->verticalScrollBar();
  CHECK(bar != nullptr);
  if (bar == nullptr) {
    return;
  }
  const auto shot = bar->grab().toImage();
  CHECK(!shot.isNull());
  if (shot.isNull()) {
    return;
  }
  save_widget_artifact("ui_light_scheme_panel_scroll_bar", *list);

  // The list sits at row 0, so the handle is parked at the top of the groove and
  // bare track runs below it. Compare the two rows across the full width rather
  // than at one column: a handle is placed by its border as much as by its fill,
  // and the fill alone sits deliberately close to the track in a light scheme.
  // What has to hold is that the two rows do not render as the same thing, which
  // is exactly what failed here.
  const int handle_y = 6;
  const int track_y = shot.height() - 6;
  int separation = 0;
  int at_column = 0;
  for (int x = 0; x < shot.width(); ++x) {
    const int delta = channel_delta(shot.pixelColor(x, handle_y), shot.pixelColor(x, track_y));
    if (delta > separation) {
      separation = delta;
      at_column = x;
    }
  }
  if (separation < 40) {
    fprintf(stderr, "  light panel scroll bar: handle row %s vs track row %s, best delta %d at x=%d, want >= 40\n",
            qPrintable(shot.pixelColor(at_column, handle_y).name(QColor::HexRgb)),
            qPrintable(shot.pixelColor(at_column, track_y).name(QColor::HexRgb)), separation,
            at_column);
  }
  CHECK(separation >= 40);
}

// The compact color picker styles its own tab bar because it wants more
// selected-tab contrast than the global theme gives, and borrowing roles to get
// it is what left it behind when the document tabs were corrected for Light. Its
// selected fill came from field_border, which sits lighter than its neighbours
// in Dark and darker than them in Light, so the current mode read as the
// inactive one. Reading the roles back would only restate whichever pairing the
// sheet happens to name, so render the bar and compare the tabs as drawn.
void ui_light_scheme_color_picker_selected_tab_reads_as_selected() {
  ColorSchemeRestorer restore_active;

  for (const auto preference :
       {patchy::ui::ColorSchemePreference::Dark, patchy::ui::ColorSchemePreference::Light}) {
    ColorSchemeRestorer::apply(preference);
    const bool light = preference == patchy::ui::ColorSchemePreference::Light;
    const char* label = light ? "light" : "dark";

    patchy::ui::PatchyColorPicker picker(QColor(120, 60, 30));
    picker.show();
    QApplication::processEvents();

    auto* tabs = picker.findChild<QTabWidget*>(QStringLiteral("patchyColorPickerTabs"));
    CHECK(tabs != nullptr);
    if (tabs == nullptr) {
      return;
    }
    auto* bar = tabs->tabBar();
    CHECK(bar != nullptr);
    if (bar == nullptr || bar->count() < 2) {
      CHECK(false);
      return;
    }
    const auto shot = bar->grab().toImage();
    CHECK(!shot.isNull());
    if (shot.isNull()) {
      return;
    }
    if (light) {
      save_widget_artifact("ui_light_scheme_color_picker", picker);
    }

    // The picker reopens on whichever mode was used last, so read the current
    // tab rather than forcing one: setting it would write that mode back into
    // the shared QSettings store for every test after this one.
    const int selected_index = bar->currentIndex();
    const int other_index = selected_index == 0 ? 1 : 0;
    // Inside the left padding, clear of the border and of the label glyphs.
    const auto fill = [&shot, bar](int index) {
      const auto rect = bar->tabRect(index);
      return shot.pixelColor(rect.left() + 5, rect.center().y());
    };
    const auto selected = fill(selected_index);
    const auto unselected = fill(other_index);

    // Lightness carries the selection in both schemes: the current mode is the
    // raised tab, whichever end of the value range the scheme lives at. The
    // threshold is what Dark already ships (0x34 against 0x5a), so this asserts
    // Light separates them at least as well.
    const int separation = selected.lightness() - unselected.lightness();
    if (separation < 30) {
      fprintf(stderr, "  %s: picker selected tab %s vs unselected %s, lightness %+d, want >= +30\n",
              label, qPrintable(selected.name(QColor::HexRgb)),
              qPrintable(unselected.name(QColor::HexRgb)), separation);
    }
    CHECK(separation >= 30);
  }
}

// The About dialog is painted from a family of near-blacks that wear a cool
// cast, and flip_for_light() reads that cast as chroma, so it clamped every one
// of them to a mid tone instead of lifting them into Light's surface band. The
// body, the secondary button and its hover all derived to within a couple of
// points of #93a4be and the dialog rendered as one flat periwinkle slab. The
// role table now says otherwise, but the other half of the bug was never in the
// table: the artwork is a plain QWidget, so the application's QWidget rule filled
// its slot with window_bg and drew a window-colored rectangle on the dialog
// surface. Only the rendered dialog shows both, so grab the real one.
void ui_light_scheme_about_dialog_reads_as_a_light_surface() {
  ColorSchemeRestorer restore_active;
  ColorSchemeRestorer::apply(patchy::ui::ColorSchemePreference::Light);

  bool inspected = false;
  QTimer::singleShot(0, [&] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchySplashScreen"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* artwork = dialog->findChild<QWidget*>(QStringLiteral("splashArtwork"));
    CHECK(artwork != nullptr);
    const auto shot = dialog->grab().toImage();
    CHECK(!shot.isNull());
    if (artwork == nullptr || shot.isNull()) {
      dialog->accept();
      return;
    }
    save_widget_artifact("ui_light_scheme_about_dialog", *dialog);

    // In the right margin below the button row: inside the 1px border, clear of
    // every child.
    const auto body = shot.pixelColor(shot.width() - 8, shot.height() - 8);
    if (body.lightness() < 210) {
      fprintf(stderr, "  light About dialog body %s, lightness %d, want >= 210\n",
              qPrintable(body.name(QColor::HexRgb)), body.lightness());
    }
    CHECK(body.lightness() >= 210);

    // A tint is allowed, a color is not. The failing body spread 43 between its
    // channels; the surface it replaced spreads single digits.
    const int spread = std::max({body.red(), body.green(), body.blue()}) -
                       std::min({body.red(), body.green(), body.blue()});
    if (spread > 16) {
      fprintf(stderr, "  light About dialog body %s spreads %d across channels, want <= 16\n",
              qPrintable(body.name(QColor::HexRgb)), spread);
    }
    CHECK(spread <= 16);

    // The artwork paints its card inside a margin, so its corner is dialog
    // surface and has to render as exactly that. Read it out of the dialog
    // image: grabbing the widget itself now returns an uninitialized pixmap,
    // which is the point of the rule under test.
    const auto slot_point = artwork->mapTo(dialog, QPoint(2, 2));
    const auto slot = shot.pixelColor(slot_point);
    const int against_body = channel_delta(slot, body);
    if (against_body > 4) {
      fprintf(stderr, "  About dialog artwork slot %s vs dialog body %s, delta %d, want <= 4\n",
              qPrintable(slot.name(QColor::HexRgb)), qPrintable(body.name(QColor::HexRgb)),
              against_body);
    }
    CHECK(against_body <= 4);

    inspected = true;
    dialog->accept();
  });

  patchy::ui::show_about_splash();
  CHECK(inspected);
}

// An unresolved @token makes Qt drop the entire declaration containing it,
// silently. Nothing downstream would notice, so assert no token survives and no
// raw hex was left behind in either scheme.
void ui_theme_qss_resolves_every_token() {
  const patchy::ui::ColorScheme entry_scheme = patchy::ui::active_color_scheme();

  for (const auto scheme : {patchy::ui::ColorScheme::Dark, patchy::ui::ColorScheme::Light}) {
    patchy::ui::set_active_color_scheme(scheme);
    const auto style = patchy::ui::photoshop_style();
    CHECK(!style.isEmpty());
    CHECK(!style.contains(QLatin1Char('@')));
  }

  patchy::ui::set_active_color_scheme(entry_scheme);

  // The template is the thing that must stay token-only: a hex literal added
  // there would be invisible to the check above.
  const auto resolved = patchy::ui::apply_theme_tokens(QStringLiteral("a: @window_bg; b: @accent;"));
  CHECK(resolved ==
        QStringLiteral("a: %1; b: %2;")
            .arg(patchy::ui::theme().window_bg.name(QColor::HexRgb),
                 patchy::ui::theme().accent.name(QColor::HexRgb)));
}

// A QFont holds its size in points OR pixels and the unused accessor returns -1.
// macOS resolves inherited widget fonts by pixel size, so the natural
// `f.setPointSizeF(f.pointSizeF() * k)` silently left every derived font at full
// size there (and spammed "Point size <= 0" through the offscreen suite). The
// pixel-defined cases below are the macOS condition, reproduced on any platform.
void ui_derived_font_sizes_scale_in_points_and_pixels() {
  QFont point_font;
  point_font.setPointSizeF(20.0);
  CHECK(std::abs(patchy::ui::scaled_font(point_font, 0.85).pointSizeF() - 17.0) < 0.0001);
  CHECK(patchy::ui::scaled_font(point_font, 0.85).pixelSize() == -1);
  CHECK(std::abs(patchy::ui::offset_font(point_font, -1, false).pointSizeF() - 19.0) < 0.0001);

  QFont pixel_font;
  pixel_font.setPixelSize(20);
  // The bug: this must shrink, not stay 20 and not become a negative point size.
  CHECK(patchy::ui::scaled_font(pixel_font, 0.85).pixelSize() == 17);
  CHECK(patchy::ui::scaled_font(pixel_font, 0.85).pointSizeF() == -1.0);
  CHECK(patchy::ui::scaled_font(pixel_font, 1.9).pixelSize() == 38);
  CHECK(patchy::ui::offset_font(pixel_font, -1, false).pixelSize() == 19);

  // Floors keep a derived size positive rather than clamping a real one.
  CHECK(patchy::ui::scaled_font(pixel_font, 0.001).pixelSize() == 1);
  QFont small_point_font;
  small_point_font.setPointSizeF(8.0);
  CHECK(patchy::ui::offset_font(small_point_font, -4, false).pointSizeF() >= 7.0);

  // Bold rides along with offset_font; scaling never touches weight.
  CHECK(patchy::ui::offset_font(point_font, 0, true).bold());
  CHECK(!patchy::ui::scaled_font(point_font, 2.0).bold());

  // A non-positive or non-finite scale is refused rather than corrupting the font.
  QFont untouched = point_font;
  patchy::ui::scale_font_size(untouched, 0.0);
  CHECK(std::abs(untouched.pointSizeF() - 20.0) < 0.0001);
  patchy::ui::scale_font_size(untouched, std::numeric_limits<double>::quiet_NaN());
  CHECK(std::abs(untouched.pointSizeF() - 20.0) < 0.0001);
}

}  // namespace

std::vector<patchy::test::TestCase> app_shell_tests() {
  return {
      {"ui_derived_font_sizes_scale_in_points_and_pixels", ui_derived_font_sizes_scale_in_points_and_pixels},
      {"ui_startup_opens_empty_workspace_with_start_panel", ui_startup_opens_empty_workspace_with_start_panel},
      {"ui_start_panel_recent_files_open_on_click", ui_start_panel_recent_files_open_on_click},
      {"ui_large_document_session_keeps_loading_responsive", ui_large_document_session_keeps_loading_responsive},
      {"ui_start_panel_recent_files_scroll_and_context_menu", ui_start_panel_recent_files_scroll_and_context_menu},
      {"ui_start_panel_recent_filter_narrows_rows_and_opens_match",
       ui_start_panel_recent_filter_narrows_rows_and_opens_match},
      {"ui_start_panel_shows_about_info_and_update_status", ui_start_panel_shows_about_info_and_update_status},
      {"ui_main_window_renders_color_controls", ui_main_window_renders_color_controls},
      {"ui_tool_palette_overflow_hides_quick_mask_before_swatches",
       ui_tool_palette_overflow_hides_quick_mask_before_swatches},
      {"ui_tool_palette_extension_button_expands_palette", ui_tool_palette_extension_button_expands_palette},
      {"ui_tool_palette_expansion_sticky_after_tool_pick", ui_tool_palette_expansion_sticky_after_tool_pick},
      {"ui_tool_palette_expanded_zoom_double_click_sets_actual_pixels",
       ui_tool_palette_expanded_zoom_double_click_sets_actual_pixels},
      {"ui_tool_palette_animated_collapse_realigns_buttons", ui_tool_palette_animated_collapse_realigns_buttons},
      {"ui_tool_palette_expansion_collapses_when_window_fits_again",
       ui_tool_palette_expansion_collapses_when_window_fits_again},
      {"ui_dissolve_clipped_render_matches_full_render", ui_dissolve_clipped_render_matches_full_render},
      {"ui_window_force_refresh_action_rebuilds_cache", ui_window_force_refresh_action_rebuilds_cache},
      {"ui_canvas_ignores_opaque_psd_flat_cache_for_first_paint_transparency",
       ui_canvas_ignores_opaque_psd_flat_cache_for_first_paint_transparency},
      {"ui_top_menu_items_highlight_on_hover", ui_top_menu_items_highlight_on_hover},
      {"ui_save_as_dialog_lists_recent_files", ui_save_as_dialog_lists_recent_files},
      {"ui_open_recent_lists_two_hundred_files_in_one_scrolling_list",
       ui_open_recent_lists_two_hundred_files_in_one_scrolling_list},
      {"ui_open_recent_filter_narrows_entries_and_opens_first_match",
       ui_open_recent_filter_narrows_entries_and_opens_first_match},
      {"ui_open_recent_folders_list_two_hundred_folders_with_filter",
       ui_open_recent_folders_list_two_hundred_folders_with_filter},
      {"ui_recent_file_context_menu_copies_path", ui_recent_file_context_menu_copies_path},
      {"ui_recent_folder_context_menu_copies_path_and_offers_explorer",
       ui_recent_folder_context_menu_copies_path_and_offers_explorer},
      {"ui_save_as_remembers_last_save_directory_between_windows",
       ui_save_as_remembers_last_save_directory_between_windows},
      {"ui_open_remembers_last_directory_and_lists_recent_folders",
       ui_open_remembers_last_directory_and_lists_recent_folders},
      {"ui_network_mount_classification_uses_deepest_mount_point",
       ui_network_mount_classification_uses_deepest_mount_point},
      {"ui_recent_history_checks_in_background_and_skips_network_paths",
       ui_recent_history_checks_in_background_and_skips_network_paths},
      {"ui_open_dialog_hides_name_filter_details", ui_open_dialog_hides_name_filter_details},
      {"ui_open_dialog_opens_every_selected_file", ui_open_dialog_opens_every_selected_file},
      {"update_manifest_parser_handles_supported_cases", update_manifest_parser_handles_supported_cases},
      {"ui_background_worker_wait_is_bounded", ui_background_worker_wait_is_bounded},
      {"ui_update_manifest_url_honors_environment_override", ui_update_manifest_url_honors_environment_override},
      {"ui_update_check_fetches_manifest_after_resolving_host", ui_update_check_fetches_manifest_after_resolving_host},
      {"ui_update_check_reports_unresolvable_host_and_drops_destroyed_owner",
       ui_update_check_reports_unresolvable_host_and_drops_destroyed_owner},
      {"ui_update_available_dialog_warns_to_close_patchy_before_installing",
       ui_update_available_dialog_warns_to_close_patchy_before_installing},
      {"update_install_location_matching_ignores_case_and_separators",
       update_install_location_matching_ignores_case_and_separators},
      {"ui_update_available_dialog_offers_one_click_update_for_installed_copy",
       ui_update_available_dialog_offers_one_click_update_for_installed_copy},
      {"ui_update_connector_restart_notice_names_the_apps", ui_update_connector_restart_notice_names_the_apps},
      {"connector_update_notice_reports_a_replaced_executable", connector_update_notice_reports_a_replaced_executable},
      {"appimage_environment_names_the_appimage_file", appimage_environment_names_the_appimage_file},
      {"ui_update_available_dialog_offers_appimage_download", ui_update_available_dialog_offers_appimage_download},
      {"update_appimage_replacement_checks_and_renames", update_appimage_replacement_checks_and_renames},
      {"ui_update_one_click_replaces_appimage_on_close", ui_update_one_click_replaces_appimage_on_close},
      {"update_installer_is_checked_again_before_it_runs", update_installer_is_checked_again_before_it_runs},
      {"ui_update_one_click_downloads_installer_and_hands_over_on_close",
       ui_update_one_click_downloads_installer_and_hands_over_on_close},
      {"ui_update_preference_defaults_startup_check_setting_to_enabled",
       ui_update_preference_defaults_startup_check_setting_to_enabled},
      {"ui_update_preference_persists_startup_check_setting", ui_update_preference_persists_startup_check_setting},
      {"ui_update_checks_can_be_switched_off", ui_update_checks_can_be_switched_off},
      {"ui_preferences_wheel_over_unfocused_control_scrolls_the_page",
       ui_preferences_wheel_over_unfocused_control_scrolls_the_page},
      {"ui_preferences_dialog_resizes_and_remembers_its_size", ui_preferences_dialog_resizes_and_remembers_its_size},
      {"ui_update_check_interval_gates_the_startup_check", ui_update_check_interval_gates_the_startup_check},
      {"ui_update_startup_check_honors_stored_interval", ui_update_startup_check_honors_stored_interval},
      {"ui_update_preference_persists_check_interval", ui_update_preference_persists_check_interval},
      {"ui_update_prompt_skip_version_silences_until_a_newer_one",
       ui_update_prompt_skip_version_silences_until_a_newer_one},
      {"ui_gui_scale_preference_persists_setting", ui_gui_scale_preference_persists_setting},
      {"ui_gui_scale_preference_persists_step_below_full_size",
       ui_gui_scale_preference_persists_step_below_full_size},
      {"ui_gui_scale_preference_falls_back_for_unknown_step",
       ui_gui_scale_preference_falls_back_for_unknown_step},
      {"gui_scale_percent_normalizes_to_offered_steps", gui_scale_percent_normalizes_to_offered_steps},
      {"ui_color_scheme_preference_persists_setting", ui_color_scheme_preference_persists_setting},
      {"ui_color_scheme_cancel_restores_entry_scheme", ui_color_scheme_cancel_restores_entry_scheme},
      {"ui_color_scheme_follow_system_tracks_style_hints",
       ui_color_scheme_follow_system_tracks_style_hints},
      {"ui_theme_file_round_trips_full_palette", ui_theme_file_round_trips_full_palette},
      {"ui_theme_file_missing_role_falls_back_to_base", ui_theme_file_missing_role_falls_back_to_base},
      {"ui_theme_file_invalid_hex_is_a_hard_error", ui_theme_file_invalid_hex_is_a_hard_error},
      {"ui_theme_file_unknown_role_is_a_warning_not_an_error",
       ui_theme_file_unknown_role_is_a_warning_not_an_error},
      {"ui_theme_file_invalid_base_is_a_hard_error", ui_theme_file_invalid_base_is_a_hard_error},
      {"ui_custom_theme_applies_over_matching_base_and_bumps_generation",
       ui_custom_theme_applies_over_matching_base_and_bumps_generation},
      {"ui_custom_theme_cleared_by_switching_to_a_builtin_scheme",
       ui_custom_theme_cleared_by_switching_to_a_builtin_scheme},
      {"ui_custom_theme_id_persists_and_reapplies_like_a_restart",
       ui_custom_theme_id_persists_and_reapplies_like_a_restart},
      {"ui_custom_theme_missing_file_falls_back_to_builtin_preference",
       ui_custom_theme_missing_file_falls_back_to_builtin_preference},
      {"ui_custom_theme_qss_resolves_every_token", ui_custom_theme_qss_resolves_every_token},
      {"ui_theme_file_newer_format_is_a_hard_error", ui_theme_file_newer_format_is_a_hard_error},
      {"ui_preferences_reload_reapplies_an_edited_theme_file", ui_preferences_reload_reapplies_an_edited_theme_file},
      {"ui_preferences_delete_removes_theme_file_and_entry", ui_preferences_delete_removes_theme_file_and_entry},
      {"ui_bundled_themes_load_and_list_in_preferences", ui_bundled_themes_load_and_list_in_preferences},
      {"ui_preferences_export_defaults_to_themes_folder_and_lists_the_copy",
       ui_preferences_export_defaults_to_themes_folder_and_lists_the_copy},
      {"ui_bundled_theme_id_persists_and_reapplies_like_a_restart",
       ui_bundled_theme_id_persists_and_reapplies_like_a_restart},
      {"ui_color_scheme_switch_updates_existing_window", ui_color_scheme_switch_updates_existing_window},
      {"ui_themed_icons_recolor_between_schemes", ui_themed_icons_recolor_between_schemes},
      {"ui_main_window_persists_window_geometry", ui_main_window_persists_window_geometry},
      {"ui_psd_import_warning_preference_defaults_to_hidden",
       ui_psd_import_warning_preference_defaults_to_hidden},
      {"ui_psd_import_warning_preference_persists_enabled_setting",
       ui_psd_import_warning_preference_persists_enabled_setting},
      {"ui_transform_shift_aspect_preference_defaults_to_off",
       ui_transform_shift_aspect_preference_defaults_to_off},
      {"ui_transform_shift_aspect_preference_persists_and_reaches_canvas",
       ui_transform_shift_aspect_preference_persists_and_reaches_canvas},
      {"ui_transform_snap_preference_persists_and_reaches_canvas",
       ui_transform_snap_preference_persists_and_reaches_canvas},
      {"ui_language_switch_updates_existing_window", ui_language_switch_updates_existing_window},
      {"ui_language_switch_survives_window_reactivation", ui_language_switch_survives_window_reactivation},
      {"ui_menubar_submenus_have_no_native_menu_role", ui_menubar_submenus_have_no_native_menu_role},
      {"ui_language_preference_applies_at_startup", ui_language_preference_applies_at_startup},
      {"ui_language_missing_preference_uses_system_language", ui_language_missing_preference_uses_system_language},
      {"ui_language_saved_preference_overrides_system_language",
       ui_language_saved_preference_overrides_system_language},
      {"ui_language_invalid_preference_falls_back_to_english", ui_language_invalid_preference_falls_back_to_english},
      {"ui_language_matching_maps_locales_to_shipped_codes", ui_language_matching_maps_locales_to_shipped_codes},
      {"ui_language_combo_lists_every_shipped_language", ui_language_combo_lists_every_shipped_language},
      {"ui_language_every_catalog_loads_and_translates", ui_language_every_catalog_loads_and_translates},
      {"ui_language_catalog_covers_dialog_status_and_properties",
       ui_language_catalog_covers_dialog_status_and_properties},
      {"ui_filter_gallery_action_retranslates", ui_filter_gallery_action_retranslates},
      {"ui_about_dialog_shows_labeled_external_links", ui_about_dialog_shows_labeled_external_links},
      {"ui_about_dialog_shows_memory_row", ui_about_dialog_shows_memory_row},
      {"ui_app_data_migration_merges_legacy_folder", ui_app_data_migration_merges_legacy_folder},
      {"ui_frameless_window_edges_resize", ui_frameless_window_edges_resize},
      {"ui_maximized_title_bar_click_keeps_window_maximized", ui_maximized_title_bar_click_keeps_window_maximized},
      {"ui_right_edge_scrollbars_remain_draggable", ui_right_edge_scrollbars_remain_draggable},
      {"ui_single_instance_socket_names_the_server_process", ui_single_instance_socket_names_the_server_process},
      {"ui_second_instance_brings_window_and_modal_dialog_forward",
       ui_second_instance_brings_window_and_modal_dialog_forward},
      {"ui_svg_icon_resources_are_registered", ui_svg_icon_resources_are_registered},
      {"ui_icon_color_map_covers_every_authored_color", ui_icon_color_map_covers_every_authored_color},
      {"ui_no_widget_ships_unresolved_theme_tokens", ui_no_widget_ships_unresolved_theme_tokens},
      {"ui_theme_palettes_define_every_role", ui_theme_palettes_define_every_role},
      {"ui_theme_adjacent_roles_stay_distinguishable", ui_theme_adjacent_roles_stay_distinguishable},
      {"ui_theme_text_roles_contrast_with_their_backgrounds",
       ui_theme_text_roles_contrast_with_their_backgrounds},
      {"ui_light_scheme_dialog_chrome_separates_from_canvas",
       ui_light_scheme_dialog_chrome_separates_from_canvas},
      {"ui_dialog_chrome_outline_reads_at_the_dialog_edge",
       ui_dialog_chrome_outline_reads_at_the_dialog_edge},
      {"ui_light_scheme_panel_scroll_bar_handle_reads_against_track",
       ui_light_scheme_panel_scroll_bar_handle_reads_against_track},
      {"ui_light_scheme_color_picker_selected_tab_reads_as_selected",
       ui_light_scheme_color_picker_selected_tab_reads_as_selected},
      {"ui_light_scheme_about_dialog_reads_as_a_light_surface",
       ui_light_scheme_about_dialog_reads_as_a_light_surface},
      {"ui_theme_qss_resolves_every_token", ui_theme_qss_resolves_every_token},
      {"ui_status_bar_error_message_flashes_then_persists_until_replaced",
       ui_status_bar_error_message_flashes_then_persists_until_replaced},
      {"ui_blocking_refusal_shows_error_status_and_info_clears_it",
       ui_blocking_refusal_shows_error_status_and_info_clears_it},
  };
}
