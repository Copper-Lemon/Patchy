// Artboards in the UI (docs/artboards.md): the Artboard tool, the Move tool's artboard
// moves and auto-nesting, the Layer and Image commands, the Layers and Properties panels,
// New Document's Artboards option, Artboards to Files, print pages, the Multi-Page PDF
// source and the script API.
#include "core/artboard.hpp"
#include "core/document.hpp"
#include "core/layer_render_utils.hpp"
#include "ui/app_settings.hpp"
#include "ui/canvas_types.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/export_documents_folder_dialog.hpp"
#include "ui/hotkey_registry.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_sequence_dialog.hpp"
#include "ui/main_window.hpp"
#include "ui/print_dialog.hpp"
#include "scripting_test_support.hpp"
#include "test_harness.hpp"
#include "ui_test_access.hpp"
#include "ui_test_groups.hpp"
#include "ui_test_support.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#if defined(PATCHY_HAVE_QT_PDF)
#include <QPdfDocument>
#endif

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace patchy::test::ui;
using patchy::ui::MainWindowTestAccess;

patchy::Document board_document(int width, int height) {
  patchy::Document document(width, height, patchy::PixelFormat::rgba8());
  document.print_settings().horizontal_ppi = 72.0;
  document.print_settings().vertical_ppi = 72.0;
  return document;
}

// A pixel layer of `rect`'s size placed at its corner.
patchy::LayerId add_solid_layer(patchy::Document& document, const char* name, QRect rect, QColor color) {
  patchy::Layer layer(document.allocate_layer_id(), name,
                      solid_pixels(rect.width(), rect.height(), patchy::PixelFormat::rgba8(), color));
  layer.set_bounds(patchy::Rect{rect.x(), rect.y(), rect.width(), rect.height()});
  const auto id = layer.id();
  document.add_layer(std::move(layer));
  return id;
}

// An artboard holding one solid layer the artboard's size, on top of the stack.
patchy::LayerId add_board(patchy::Document& document, const char* name, patchy::Rect rect,
                          patchy::ArtboardBackground background, QColor content) {
  auto artboard = patchy::make_artboard_group(document.allocate_layer_id(), name, rect, background);
  if (content.isValid()) {
    patchy::Layer child(document.allocate_layer_id(), std::string(name) + " Art",
                        solid_pixels(rect.width / 2, rect.height / 2, patchy::PixelFormat::rgba8(), content));
    child.set_bounds(patchy::Rect{rect.x, rect.y, rect.width / 2, rect.height / 2});
    artboard.add_child(std::move(child));
  }
  const auto id = artboard.id();
  document.layers().push_back(std::move(artboard));
  document.metadata().artboard_settings.emplace();
  return id;
}

const patchy::Layer* top_level(const patchy::Document& document, const char* name) {
  for (const auto& layer : document.layers()) {
    if (layer.name() == name) {
      return &layer;
    }
  }
  return nullptr;
}

void canvas_drag(patchy::ui::CanvasWidget& canvas, QPoint from, QPoint to,
                 Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
  drag_document_path(canvas, {from, to}, 6, modifiers);
  QApplication::processEvents();
}

// Repeatedly runs `step` on a short timer while a modal dialog's exec() blocks.
void drive_modal(const std::shared_ptr<std::function<bool()>>& step, int attempts = 2400) {
  QTimer::singleShot(25, [step, attempts] {
    if ((*step)()) {
      return;
    }
    if (attempts > 0) {
      drive_modal(step, attempts - 1);
    }
  });
}

// The Artboard tool end to end: a drag on empty canvas makes an artboard, a corner handle
// resizes it, a drag inside moves it, arrow keys nudge (one undo step per run), and an
// Alt-drag past the canvas edge copies it and grows the canvas. Every edit undoes.
void ui_artboard_tool_creates_resizes_moves_and_undoes() {
  SettingsValueRestorer background_restorer(QStringLiteral("tools/artboardBackground"));
  patchy::ui::app_settings().setValue(QStringLiteral("tools/artboardBackground"), QStringLiteral("white"));
  patchy::ui::MainWindow window;
  show_window(window);
  window.add_document_session(board_document(300, 200), QStringLiteral("Boards"));
  auto* canvas = require_canvas(window);
  canvas->set_zoom(1.0);
  QApplication::processEvents();
  require_hotkey_action(window, QStringLiteral("tools.artboard"))->trigger();
  CHECK(canvas->tool() == patchy::ui::CanvasTool::Artboard);
  const auto& document = [&window]() -> const patchy::Document& { return MainWindowTestAccess::document(window); };
  const auto rect_of = [&](std::size_t index) {
    const auto artboards = patchy::document_artboards(document());
    return index < artboards.size() ? artboards[index]->artboard()->rect : patchy::Rect{};
  };

  canvas_drag(*canvas, {20, 30}, {120, 110});
  auto artboards = patchy::document_artboards(document());
  CHECK(artboards.size() == 1);
  if (artboards.size() != 1) {
    return;
  }
  CHECK(artboards[0]->name() == "Artboard 1");
  CHECK((rect_of(0) == patchy::Rect{20, 30, 100, 80}));
  CHECK(artboards[0]->artboard()->background == patchy::ArtboardBackground::White);
  CHECK(artboards[0]->blend_mode() == patchy::BlendMode::Normal);
  MainWindowTestAccess::undo(window);
  CHECK(patchy::document_artboards(document()).empty());
  MainWindowTestAccess::redo(window);
  CHECK(patchy::document_artboards(document()).size() == 1);

  // The bottom-right handle of the selected artboard.
  canvas_drag(*canvas, {120, 110}, {150, 130});
  CHECK((rect_of(0) == patchy::Rect{20, 30, 130, 100}));
  // A drag inside moves it.
  canvas_drag(*canvas, {60, 60}, {80, 70});
  CHECK((rect_of(0) == patchy::Rect{40, 40, 130, 100}));
  MainWindowTestAccess::undo(window);
  CHECK((rect_of(0) == patchy::Rect{20, 30, 130, 100}));
  MainWindowTestAccess::redo(window);

  // Three nudges, one undo step.
  const auto depth = MainWindowTestAccess::active_session_undo_depth(window);
  for (int i = 0; i < 3; ++i) {
    send_key(*canvas, Qt::Key_Right);
  }
  CHECK(rect_of(0).x == 43);
  CHECK(MainWindowTestAccess::active_session_undo_depth(window) == depth + 1);
  MainWindowTestAccess::undo(window);
  CHECK(rect_of(0).x == 40);

  // Alt-drag copies; past the right edge the canvas grows to hold the copy.
  canvas_drag(*canvas, {60, 60}, {260, 60}, Qt::AltModifier);
  artboards = patchy::document_artboards(document());
  CHECK(artboards.size() == 2);
  if (artboards.size() == 2) {
    // The copy lands about 200 px right (the move snaps its center to the canvas edge);
    // the source stays.
    const auto& copy = artboards[0]->artboard()->rect.x > 100 ? *artboards[0] : *artboards[1];
    const auto& source = artboards[0]->artboard()->rect.x > 100 ? *artboards[1] : *artboards[0];
    CHECK(std::abs(copy.artboard()->rect.x - 240) <= 8 && copy.artboard()->rect.y == 40);
    CHECK((source.artboard()->rect == patchy::Rect{40, 40, 130, 100}));
    CHECK(document().width() == copy.artboard()->rect.x + 130);
  }
  // The other tools forget a half-made drag.
  require_hotkey_action(window, QStringLiteral("tools.move"))->trigger();
  CHECK(!canvas->artboard_drag_active());
}

// Layer > New > Artboard from Layers, Convert Artboard to Group, Artboard from Group and
// Image > Fit Canvas to Artboards, each one undoable step.
void ui_artboard_layer_commands_and_fit_canvas() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto seed = board_document(300, 200);
  const auto a = add_solid_layer(seed, "A", QRect(10, 10, 50, 40), QColor(255, 0, 0));
  const auto b = add_solid_layer(seed, "B", QRect(100, 50, 40, 40), QColor(0, 0, 255));
  window.add_document_session(std::move(seed), QStringLiteral("Commands"));
  QApplication::processEvents();
  const auto& document = [&window]() -> const patchy::Document& { return MainWindowTestAccess::document(window); };

  MainWindowTestAccess::select_layers(window, {a, b}, b);
  require_hotkey_action(window, QStringLiteral("layer.artboard_from_layers"))->trigger();
  auto artboards = patchy::document_artboards(document());
  CHECK(artboards.size() == 1);
  if (artboards.size() != 1) {
    return;
  }
  const auto board_id = artboards[0]->id();
  CHECK((artboards[0]->artboard()->rect == patchy::Rect{10, 10, 130, 80}));
  CHECK(artboards[0]->children().size() == 2);
  CHECK(artboards[0]->children().size() == 2 && artboards[0]->children()[0].id() == a &&
        artboards[0]->children()[1].id() == b);
  CHECK(document().layers().size() == 1);

  MainWindowTestAccess::select_layers(window, {board_id}, board_id);
  require_hotkey_action(window, QStringLiteral("layer.artboard_to_group"))->trigger();
  const auto* group = document().find_layer(board_id);
  CHECK(group != nullptr && group->kind() == patchy::LayerKind::Group && !group->artboard().has_value());
  CHECK(group != nullptr && group->children().size() == 2);

  MainWindowTestAccess::select_layers(window, {board_id}, board_id);
  require_hotkey_action(window, QStringLiteral("layer.artboard_from_group"))->trigger();
  group = document().find_layer(board_id);
  CHECK(group != nullptr && group->artboard().has_value());
  CHECK(group != nullptr && group->artboard().has_value() &&
        (group->artboard()->rect == patchy::Rect{10, 10, 130, 80}));

  require_hotkey_action(window, QStringLiteral("image.fit_canvas_to_artboards"))->trigger();
  CHECK(document().width() == 130 && document().height() == 80);
  group = document().find_layer(board_id);
  CHECK(group != nullptr && group->artboard().has_value() &&
        (group->artboard()->rect == patchy::Rect{0, 0, 130, 80}));
  // The content moved with the canvas: A's corner is the artboard's.
  const auto* moved_a = document().find_layer(a);
  CHECK(moved_a != nullptr && moved_a->bounds().x == 0 && moved_a->bounds().y == 0);
  MainWindowTestAccess::undo(window);
  CHECK(document().width() == 300 && document().height() == 200);
}

// The Move tool moves a selected artboard with its content; with Auto-Nest on, a layer
// dragged onto an artboard joins it and one dragged off every artboard leaves.
void ui_artboard_move_tool_moves_artboards_and_auto_nests() {
  SettingsValueRestorer nest_restorer(QStringLiteral("tools/artboardAutoNest"));
  patchy::ui::app_settings().setValue(QStringLiteral("tools/artboardAutoNest"), true);
  patchy::ui::MainWindow window;
  show_window(window);
  auto seed = board_document(400, 200);
  const auto loose = add_solid_layer(seed, "Loose", QRect(250, 20, 40, 40), QColor(0, 160, 0));
  const auto board = add_board(seed, "Board", patchy::Rect{0, 0, 150, 150}, patchy::ArtboardBackground::White,
                               QColor(200, 0, 0));
  window.add_document_session(std::move(seed), QStringLiteral("Move"));
  auto* canvas = require_canvas(window);
  canvas->set_zoom(1.0);
  QApplication::processEvents();
  require_hotkey_action(window, QStringLiteral("tools.move"))->trigger();
  const auto& document = [&window]() -> const patchy::Document& { return MainWindowTestAccess::document(window); };

  MainWindowTestAccess::select_layers(window, {board}, board);
  canvas_drag(*canvas, {50, 50}, {70, 60});
  const auto* artboard = document().find_layer(board);
  CHECK(artboard != nullptr && (artboard->artboard()->rect == patchy::Rect{20, 10, 150, 150}));
  CHECK(artboard != nullptr && !artboard->children().empty() && artboard->children()[0].bounds().x == 20 &&
        artboard->children()[0].bounds().y == 10);

  // Loose layer onto the artboard: it nests.
  MainWindowTestAccess::select_layers(window, {loose}, loose);
  canvas_drag(*canvas, {270, 40}, {80, 80});
  CHECK(patchy::artboard_owning_layer(document(), loose) != nullptr &&
        patchy::artboard_owning_layer(document(), loose)->id() == board);
  // And back off every artboard: it returns to the root.
  MainWindowTestAccess::select_layers(window, {loose}, loose);
  canvas_drag(*canvas, {80, 80}, {300, 60});
  CHECK(patchy::artboard_owning_layer(document(), loose) == nullptr);
  CHECK(top_level(document(), "Loose") != nullptr);
}

// The Layers panel marks artboard rows; the Properties panel shows the selected
// artboard's rect and moves or resizes it.
void ui_artboard_layers_panel_rows_and_properties_panel() {
  patchy::ui::MainWindow window;
  show_window(window);
  auto seed = board_document(300, 200);
  const auto board = add_board(seed, "Card", patchy::Rect{10, 20, 120, 80}, patchy::ArtboardBackground::Black,
                               QColor(255, 255, 0));
  window.add_document_session(std::move(seed), QStringLiteral("Panels"));
  QApplication::processEvents();
  MainWindowTestAccess::select_layers(window, {board}, board);
  QApplication::processEvents();
  int artboard_rows = 0;
  for (auto* row : window.findChildren<QWidget*>(QStringLiteral("layerRowWidget"))) {
    artboard_rows += row->property("layerRowArtboard").toBool() ? 1 : 0;
  }
  CHECK(artboard_rows == 1);

  auto* panel = window.findChild<QWidget*>(QStringLiteral("propertiesArtboardPanel"));
  auto* x = window.findChild<QSpinBox*>(QStringLiteral("propertiesArtboardXSpin"));
  auto* width = window.findChild<QSpinBox*>(QStringLiteral("propertiesArtboardWidthSpin"));
  auto* background = window.findChild<QComboBox*>(QStringLiteral("propertiesArtboardBackgroundCombo"));
  CHECK(panel != nullptr && x != nullptr && width != nullptr && background != nullptr);
  if (panel == nullptr || x == nullptr || width == nullptr || background == nullptr) {
    return;
  }
  CHECK(!panel->isHidden());
  CHECK(x->value() == 10 && width->value() == 120);
  CHECK(background->currentData().toString() == QStringLiteral("black"));
  x->setValue(25);
  QApplication::processEvents();
  const auto& document = MainWindowTestAccess::document(window);
  const auto* artboard = std::as_const(document).find_layer(board);
  CHECK(artboard != nullptr && (artboard->artboard()->rect == patchy::Rect{25, 20, 120, 80}));
  CHECK(artboard != nullptr && artboard->children()[0].bounds().x == 25);  // the content travels
  width->setValue(140);
  QApplication::processEvents();
  artboard = std::as_const(document).find_layer(board);
  CHECK(artboard != nullptr && (artboard->artboard()->rect == patchy::Rect{25, 20, 140, 80}));
  // A plain layer selected: the artboard section hides.
  const auto child = artboard->children()[0].id();
  MainWindowTestAccess::select_layers(window, {child}, child);
  QApplication::processEvents();
  CHECK(panel->isHidden());
}

// New Document's Artboards option starts the document as one artboard the canvas size,
// holding "Layer 1".
void ui_artboard_new_document_option() {
  SettingsValueRestorer option_restorer(QStringLiteral("newDocument/artboards"));
  // An image left on the clipboard by an earlier test would preselect the Clipboard card,
  // which makes a document from the clipboard instead.
  QGuiApplication::clipboard()->clear();
  patchy::ui::MainWindow window;
  show_window(window);
  auto done = std::make_shared<bool>(false);
  auto step = std::make_shared<std::function<bool()>>();
  *step = [done] {
    auto* dialog = find_top_level_dialog(QStringLiteral("patchyNewDocumentDialog"));
    if (dialog == nullptr) {
      return false;
    }
    auto* artboards = dialog->findChild<QCheckBox*>(QStringLiteral("newDocumentArtboardsCheck"));
    auto* create = dialog->findChild<QPushButton*>(QStringLiteral("newDocumentCreateButton"));
    if (artboards == nullptr || create == nullptr) {
      return false;
    }
    artboards->setChecked(true);
    create->click();
    *done = true;
    return true;
  };
  drive_modal(step);
  require_hotkey_action(window, QStringLiteral("file.new"))->trigger();
  CHECK(*done);
  const auto& document = std::as_const(MainWindowTestAccess::document(window));
  const auto artboards = patchy::document_artboards(document);
  CHECK(artboards.size() == 1);
  if (artboards.size() == 1) {
    CHECK(artboards[0]->name() == "Artboard 1");
    CHECK((artboards[0]->artboard()->rect == patchy::Rect{0, 0, document.width(), document.height()}));
    CHECK(artboards[0]->children().size() == 1 && artboards[0]->children()[0].name() == "Layer 1");
  }
  CHECK(patchy::ui::app_settings().value(QStringLiteral("newDocument/artboards")).toBool());
}

// File > Export > Artboards to Files writes one file per artboard named for it, at the
// artboard's size, never overwriting in Add mode; Print's artboard pages write one sheet
// per artboard, each its own size when the paper matches.
void ui_artboard_exports_to_files_and_print_pages() {
  QTemporaryDir temp;
  CHECK(temp.isValid());
  SettingsValueRestorer last_save_restorer(QStringLiteral("lastSaveDirectory"));
  patchy::ui::MainWindow window;
  show_window(window);
  auto seed = board_document(400, 200);
  const auto back = add_board(seed, "Back", patchy::Rect{250, 0, 100, 150}, patchy::ArtboardBackground::Black,
                              QColor());
  const auto front = add_board(seed, "Front", patchy::Rect{0, 0, 150, 100}, patchy::ArtboardBackground::White,
                               QColor(255, 0, 0));
  window.add_document_session(std::move(seed), QStringLiteral("Card"));
  QApplication::processEvents();
  CHECK(require_hotkey_action(window, QStringLiteral("file.export_artboards")) != nullptr);

  patchy::ui::ImageSequenceNaming naming;
  naming.use_layer_names = true;
  naming.prefix = QStringLiteral("Card - ");
  const auto written = MainWindowTestAccess::export_artboards_to_folder(
      window, {front, back}, temp.path(), QStringLiteral("png"), naming,
      patchy::ui::ExportDocumentsExistingFiles::AddNumbering);
  CHECK(written.has_value() && written->size() == 2);
  if (written.has_value() && written->size() == 2) {
    CHECK(QFileInfo(written->at(0)).fileName() == QStringLiteral("Card - Front.png"));
    CHECK(QFileInfo(written->at(1)).fileName() == QStringLiteral("Card - Back.png"));
    const QImage front_image(written->at(0));
    const QImage back_image(written->at(1));
    CHECK(front_image.size() == QSize(150, 100) && back_image.size() == QSize(100, 150));
    CHECK(front_image.pixelColor(20, 20) == QColor(255, 0, 0));
    CHECK(front_image.pixelColor(120, 80) == QColor(255, 255, 255));
    CHECK(back_image.pixelColor(50, 75) == QColor(0, 0, 0));
  }
  const auto again = MainWindowTestAccess::export_artboards_to_folder(
      window, {front}, temp.path(), QStringLiteral("png"), naming,
      patchy::ui::ExportDocumentsExistingFiles::AddNumbering);
  CHECK(again.has_value() && again->size() == 1 &&
        QFileInfo(again->front()).fileName() == QStringLiteral("Card - Front 2.png"));

  const auto& document = std::as_const(MainWindowTestAccess::document(window));
  auto settings = patchy::ui::default_print_settings(document, std::nullopt);
  settings.area_mode = patchy::ui::PrintAreaMode::Artboards;
  settings.match_artboard_paper = true;
  const auto pages = patchy::ui::print_artboard_pages(document);
  CHECK(pages.size() == 2);
  const auto sheet = patchy::ui::artboard_page_layout(pages.front()).fullRect(QPageLayout::Point);
  CHECK(std::abs(sheet.width() - 150.0) < 0.5 && std::abs(sheet.height() - 100.0) < 0.5);
  // Each page prints whole, at actual size, centered.
  const auto page_settings = patchy::ui::artboard_page_print_settings(settings);
  CHECK(page_settings.area_mode == patchy::ui::PrintAreaMode::Document &&
        page_settings.scale_mode == patchy::ui::PrintScaleMode::ActualSize && page_settings.center);
#if defined(PATCHY_HAVE_QT_PDF)
  const auto pdf_path = temp.filePath(QStringLiteral("print.pdf"));
  CHECK(patchy::ui::write_print_pdf(pdf_path, document, settings, patchy::ui::default_print_page_layout(),
                                    QStringLiteral("Card")));
  {
    QPdfDocument reader;
    CHECK(reader.load(pdf_path) == QPdfDocument::Error::None);
    CHECK(reader.pageCount() == 2);
    if (reader.pageCount() == 2) {
      CHECK(std::abs(reader.pagePointSize(0).width() - 150.0) < 1.0);
      CHECK(std::abs(reader.pagePointSize(1).height() - 150.0) < 1.0);
    }
  }
  // On the chosen paper instead: two Letter sheets.
  settings.match_artboard_paper = false;
  const auto letter_path = temp.filePath(QStringLiteral("print-letter.pdf"));
  CHECK(patchy::ui::write_print_pdf(letter_path, document, settings, patchy::ui::default_print_page_layout(),
                                    QStringLiteral("Card")));
  QPdfDocument letter;
  CHECK(letter.load(letter_path) == QPdfDocument::Error::None);
  CHECK(letter.pageCount() == 2);
  CHECK(letter.pageCount() != 2 || std::abs(letter.pagePointSize(1).width() - 612.0) < 1.0);
#endif
}

// Export > Multi-Page PDF offers the active document's artboards as its pages, and
// starts on that source for an artboard document.
void ui_artboard_multipage_pdf_source() {
  SettingsValueRestorer source_restorer(QStringLiteral("exportOptions/multiPagePdfSource"));
  patchy::ui::MainWindow window;
  show_window(window);
  auto seed = board_document(300, 200);
  add_board(seed, "One", patchy::Rect{0, 0, 100, 100}, patchy::ArtboardBackground::White, QColor(0, 0, 255));
  add_board(seed, "Two", patchy::Rect{150, 0, 100, 60}, patchy::ArtboardBackground::White, QColor());
  window.add_document_session(std::move(seed), QStringLiteral("Pages"));
  QApplication::processEvents();
  auto seen = std::make_shared<bool>(false);
  QTimer::singleShot(0, [seen] {
    auto* dialog = find_top_level_dialog(QStringLiteral("multiPagePdfExportDialog"));
    CHECK(dialog != nullptr);
    if (dialog == nullptr) {
      return;
    }
    auto* artboards = dialog->findChild<QRadioButton*>(QStringLiteral("multiPagePdfArtboardsRadio"));
    CHECK(artboards != nullptr && artboards->isEnabled() && artboards->isChecked());
    CHECK(artboards != nullptr && artboards->text().contains(QStringLiteral("2")));
    *seen = true;
    dialog->reject();
  });
  require_action(window, "fileExportMultiPagePdfAction")->trigger();
  CHECK(*seen);
}

// The commands and tool are registered under their permanent ids, the Move flyout cycles
// on Shift+V, and the menus hold every command.
void ui_artboard_commands_and_hotkeys_registered() {
  patchy::ui::MainWindow window;
  show_window(window);
  for (const char* id : {"tools.artboard", "tools.cycle.move", "layer.new_artboard", "layer.artboard_from_group",
                         "layer.artboard_from_layers", "layer.artboard_to_group", "image.fit_canvas_to_artboards",
                         "file.export_artboards"}) {
    const auto* command = window.hotkey_registry().find_command(QString::fromLatin1(id));
    CHECK(command != nullptr && command->action != nullptr);
  }
  auto* cycle = require_hotkey_action(window, QStringLiteral("tools.cycle.move"));
  CHECK(cycle != nullptr && cycle->shortcut() == QKeySequence(Qt::SHIFT | Qt::Key_V));
  for (const char* name : {"layerNewArtboardAction", "layerArtboardFromGroupAction", "layerArtboardFromLayersAction",
                           "layerArtboardToGroupAction", "fileExportArtboardsAction"}) {
    auto* action = require_action(window, name);
    CHECK(action != nullptr && action->menuRole() == QAction::NoRole);
  }
}

// The script API: add, read, move, restyle, wrap layers, fit, export and refuse what
// artboards cannot do.
void ui_script_artboards_add_move_and_export() {
  QTemporaryDir temp;
  CHECK(temp.isValid());
  patchy::ui::MainWindow window;
  show_window(window);
  const auto folder = QDir::fromNativeSeparators(temp.path());
  const auto pdf_path = folder + QStringLiteral("/boards.pdf");
  const auto quoted = [](const QString& text) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact)).chopped(1).mid(1);
  };
  CHECK(scripting_support::run_script(window, QStringLiteral(R"JS(
    var doc = app.newDocument(100, 100);
    var a = doc.addArtboard('Phone', {x: 0, y: 0, width: 120, height: 200});
    var b = doc.addArtboard('', {x: 220, y: 0, width: 80, height: 80}, {color: '#336699'});
    var names = doc.artboards.map(function (x) { return x.name; });
    if (names.length !== 2 || names[0] !== b.name || b.name !== 'Artboard 1') throw new Error('order ' + names);
    if (!a.isArtboard || a.artboard.width !== 120 || a.artboard.background !== 'white') throw new Error('a ' + JSON.stringify(a.artboard));
    if (b.artboard.background !== 'color' || b.artboard.color !== '#336699') throw new Error('b ' + JSON.stringify(b.artboard));
    if (doc.width !== 300 || doc.height !== 200) throw new Error('canvas ' + doc.width + 'x' + doc.height);
    a.moveTo(10, 0);
    if (a.x !== 10 || a.bounds.x !== 10 || a.artboard.x !== 10) throw new Error('move ' + a.x);
    a.setArtboard({width: 150, background: 'black'});
    if (a.artboard.width !== 150 || a.artboard.background !== 'black') throw new Error('restyle');
    var paint = doc.addLayer('Paint');
    paint.fillRect(0, 0, 10, 10, '#ff0000');
    var wrapped = doc.artboardFromLayers([paint], 'Wrapped');
    if (!wrapped.isArtboard || wrapped.name !== 'Wrapped' || wrapped.children.length !== 1) throw new Error('wrap');
    var threw = false;
    try { doc.groupLayers([a], 'G'); } catch (e) { threw = true; }
    if (!threw) throw new Error('artboard grouped');
    threw = false;
    try { doc.addArtboard('x', {x: 0, y: 0, width: 0, height: 5}); } catch (e) { threw = true; }
    if (!threw) throw new Error('empty artboard accepted');
    var shift = doc.fitCanvasToArtboards();
    if (typeof shift.x !== 'number' || typeof shift.y !== 'number') throw new Error('shift');
    var files = doc.exportArtboards(%1, {format: 'png', prefix: 'S - ', artboards: [a, b]});
    if (files.length !== 2 || files[0].indexOf('S - Phone.png') < 0) throw new Error('files ' + files);
    threw = false;
    try { doc.exportArtboards(%1, {format: 'nope'}); } catch (e) { threw = true; }
    if (!threw) throw new Error('bad format accepted');
    doc.exportAs(%2);
    wrapped.setArtboard(null);
    if (wrapped.isArtboard || wrapped.artboard !== null) throw new Error('unset');
    console.log('artboards-ok ' + doc.artboards.length);
  )JS")
                                                 .arg(quoted(folder), quoted(pdf_path))));
  CHECK(scripting_support::backlog_contains(window, QStringLiteral("artboards-ok 2")));
  CHECK(QImage(folder + QStringLiteral("/S - Phone.png")).size() == QSize(150, 200));
  CHECK(QImage(folder + QStringLiteral("/S - Artboard 1.png")).pixelColor(40, 40) == QColor(0x33, 0x66, 0x99));
#if defined(PATCHY_HAVE_QT_PDF)
  QPdfDocument reader;
  CHECK(reader.load(pdf_path) == QPdfDocument::Error::None);
  CHECK(reader.pageCount() == 3);  // Wrapped, Artboard 1, Phone: top of the stack first
#endif
}


// The drag shows what the release does (docs/artboards.md, "Fast paths"): a red layer
// dragged from artboard A into artboard B draws over B's paper mid-drag and leaves A's
// paper clean, at 100% and 50% zoom (the preview-scaled document), with live patches and
// with the latched proxy; with Auto-Nest off it stays clipped to A, mid-drag and after.
void ui_artboard_drag_across_artboards_previews_the_release() {
  EnvironmentVariableRestorer restore_latch("PATCHY_MOVE_LIVE_LATCH_MS");
  struct Case {
    double zoom;
    bool proxy;
    bool auto_nest;
  };
  for (const auto& run : {Case{1.0, false, true}, Case{0.5, false, true}, Case{1.0, true, true},
                          Case{0.5, true, true}, Case{1.0, false, false}}) {
    if (run.proxy) {
      qputenv("PATCHY_MOVE_LIVE_LATCH_MS", QByteArray("0"));
    } else {
      qunsetenv("PATCHY_MOVE_LIVE_LATCH_MS");
    }
    auto document = board_document(360, 160);
    const auto board_a = add_board(document, "A", patchy::Rect{0, 0, 140, 140}, patchy::ArtboardBackground::White,
                                   QColor());
    const auto board_b = add_board(document, "B", patchy::Rect{200, 0, 140, 140}, patchy::ArtboardBackground::White,
                                   QColor());
    patchy::Layer red(document.allocate_layer_id(), "Red",
                      solid_pixels(24, 24, patchy::PixelFormat::rgba8(), QColor(220, 30, 30)));
    red.set_bounds(patchy::Rect{20, 20, 24, 24});
    const auto red_id = red.id();
    for (auto& layer : document.layers()) {
      if (layer.id() == board_a) {
        layer.add_child(std::move(red));
      }
    }

    patchy::ui::CanvasWidget canvas;
    canvas.resize(820, 420);
    canvas.set_document(&document);
    canvas.set_zoom(run.zoom);
    canvas.set_tool(patchy::ui::CanvasTool::Move);
    canvas.set_show_transform_controls(false);
    canvas.set_auto_select_layer(false);
    canvas.set_snap_enabled(false);
    canvas.set_artboard_auto_nest(run.auto_nest);
    canvas.set_selected_layer_ids({red_id});
    canvas.show();
    QApplication::processEvents();

    // From the red square's center (32, 32) to (252, 32): into B, 220 px right.
    const auto start = canvas.widget_position_for_document_point(QPoint(32, 32));
    const auto end = canvas.widget_position_for_document_point(QPoint(252, 32));
    send_mouse(canvas, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 6; ++step) {
      send_mouse(canvas, QEvent::MouseMove, start + (end - start) * step / 6, Qt::NoButton, Qt::LeftButton);
      QApplication::processEvents();
    }
    if (run.auto_nest) {
      // Visual QA: the mid-drag frame (test-artifacts/ui_artboard_drag_mid_*.png).
      save_image_artifact(std::string("ui_artboard_drag_mid_") + (run.zoom < 1.0 ? "50" : "100") +
                              (run.proxy ? "_proxy" : "_live"),
                          canvas.grab().toImage());
    }
    const auto mid_destination = canvas_pixel(canvas, QPoint(252, 32));
    const auto mid_origin = canvas_pixel(canvas, QPoint(32, 32));
    CHECK(color_close(mid_origin, QColor(Qt::white), 30));  // A's paper, the square gone
    if (run.auto_nest) {
      CHECK(color_close(mid_destination, QColor(220, 30, 30), 45));  // drawn on B, over its paper
    } else {
      CHECK(color_close(mid_destination, QColor(Qt::white), 30));  // still A's: clipped away
    }
    send_mouse(canvas, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    QApplication::processEvents();
    const auto* owner = patchy::artboard_owning_layer(std::as_const(document), red_id);
    CHECK(owner != nullptr && owner->id() == (run.auto_nest ? board_b : board_a));
    // The released frame matches the drag's last frame.
    const auto after = canvas_pixel(canvas, QPoint(252, 32));
    CHECK(color_close(after, mid_destination, 30));
  }
}

}  // namespace

std::vector<patchy::test::TestCase> artboard_ui_tests() {
  return {
      {"ui_artboard_tool_creates_resizes_moves_and_undoes", ui_artboard_tool_creates_resizes_moves_and_undoes},
      {"ui_artboard_layer_commands_and_fit_canvas", ui_artboard_layer_commands_and_fit_canvas},
      {"ui_artboard_move_tool_moves_artboards_and_auto_nests", ui_artboard_move_tool_moves_artboards_and_auto_nests},
      {"ui_artboard_layers_panel_rows_and_properties_panel", ui_artboard_layers_panel_rows_and_properties_panel},
      {"ui_artboard_new_document_option", ui_artboard_new_document_option},
      {"ui_artboard_exports_to_files_and_print_pages", ui_artboard_exports_to_files_and_print_pages},
      {"ui_artboard_multipage_pdf_source", ui_artboard_multipage_pdf_source},
      {"ui_artboard_commands_and_hotkeys_registered", ui_artboard_commands_and_hotkeys_registered},
      {"ui_script_artboards_add_move_and_export", ui_script_artboards_add_move_and_export},
      {"ui_artboard_drag_across_artboards_previews_the_release", ui_artboard_drag_across_artboards_previews_the_release},
  };
}
