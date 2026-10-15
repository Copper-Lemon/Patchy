#include "ui/main_window.hpp"
#include "ui/main_window_shared.hpp"

#include "core/layer_metadata.hpp"
#include "core/smart_object.hpp"
#include "core/text_warp.hpp"
#include "core/warp_mesh.hpp"
#include "core/layer_render_utils.hpp"
#include "core/layer_tree.hpp"
#include "core/palette_presets.hpp"
#include "core/pixel_tools.hpp"
#include "formats/palette_io.hpp"
#include "filters/builtin_filters.hpp"
#include "formats/aseprite_document_io.hpp"
#include "formats/bmp_document_io.hpp"
#include "plugins/legacy_photoshop_adapter.hpp"
#include "psd/psd_document_io.hpp"
#include "psd/psd_smart_objects.hpp"
#include "ui/action_icons.hpp"
#include "ui/app_settings.hpp"
#include "render/compositor.hpp"
#include "core/pixel_depth.hpp"
#include "ui/blend_mode_ui.hpp"
#include "ui/brush_dynamics_popup.hpp"
#include "ui/brush_presets.hpp"
#include "ui/brush_tip_library.hpp"
#include "ui/brush_tip_manager_dialog.hpp"
#include "ui/brush_tip_picker.hpp"
#include "ui/default_brush_tips.hpp"
#include "ui/compatibility_report.hpp"
#include "ui/image_document_io.hpp"
#include "ui/image_save_options_dialog.hpp"
#include "ui/filter_workflows.hpp"
#include "ui/gradient_stops_editor.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/document_float_window.hpp"
#include "ui/font_picker.hpp"
#include "ui/hotkey_editor.hpp"
#include "ui/edit_conversions.hpp"
#include "ui/color_panel.hpp"
#include "ui/layer_style_dialog.hpp"
#include "ui/layer_list_widget.hpp"
#include "ui/localization.hpp"
#include "ui/palette_convert_dialog.hpp"
#include "ui/palette_panel.hpp"
#include "ui/print_dialog.hpp"
#include "ui/smart_object_render.hpp"
#include "ui/scanner_import.hpp"
#include "ui/image_sequence_dialog.hpp"
#include "ui/sprite_sheet_dialog.hpp"
#include "ui/tile_preview_window.hpp"
#include "ui/warp_text_dialog.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/splash_dialog.hpp"
#include "ui/update_checker.hpp"
#include "ui/zoom_status_bar.hpp"
#include "support/string_utils.hpp"

#include <QAbstractItemView>
#include <QAbstractItemModel>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBrush>
#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QDateTime>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLayout>
#include <QResizeEvent>
#include <QIcon>
#include <QImageReader>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QKeySequence>
#include <QListWidget>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygon>
#include <QPointer>
#include <QProcess>
#include <QProgressDialog>
#include <QRegion>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QScopeGuard>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QMutex>
#include <QRawFont>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextLayout>
#include <QTextOption>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QTransform>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <tchar.h>
#include <tpcshrd.h>
#endif

// Icon resources live in the static patchy_ui library; force registration before first use.
int qInitResources_icons();

namespace patchy::ui {

namespace {

QPixmap component_channel_thumbnail(QColor color) {
  QPixmap result(42, 30);
  result.fill(QColor(30, 32, 35));
  QPainter painter(&result);
  const QRect swatch(3, 3, result.width() - 6, result.height() - 6);
  QLinearGradient gradient(swatch.topLeft(), swatch.topRight());
  gradient.setColorAt(0.0, Qt::black);
  gradient.setColorAt(1.0, color);
  painter.fillRect(swatch, gradient);
  painter.setPen(QColor(92, 96, 102));
  painter.drawRect(swatch.adjusted(0, 0, -1, -1));
  return result;
}

// An ink plate's swatch: paper white to the ink.
QPixmap ink_plate_thumbnail(QColor ink) {
  QPixmap result(42, 30);
  result.fill(QColor(30, 32, 35));
  QPainter painter(&result);
  const QRect swatch(3, 3, result.width() - 6, result.height() - 6);
  QLinearGradient gradient(swatch.topLeft(), swatch.topRight());
  gradient.setColorAt(0.0, Qt::white);
  gradient.setColorAt(1.0, ink);
  painter.fillRect(swatch, gradient);
  painter.setPen(QColor(92, 96, 102));
  painter.drawRect(swatch.adjusted(0, 0, -1, -1));
  return result;
}

[[nodiscard]] bool is_ink_plate_row(ChannelPanel::RowKind kind) {
  return kind == ChannelPanel::RowKind::Cyan || kind == ChannelPanel::RowKind::Magenta ||
         kind == ChannelPanel::RowKind::Yellow || kind == ChannelPanel::RowKind::Black;
}

QPixmap grayscale_channel_thumbnail(const PixelBuffer& pixels) {
  if (pixels.empty() || pixels.format() != PixelFormat::gray8()) {
    return {};
  }
  QImage image(pixels.width(), pixels.height(), QImage::Format_Grayscale8);
  for (int y = 0; y < pixels.height(); ++y) {
    const auto source = pixels.row(y);
    std::copy(source.begin(), source.end(), image.scanLine(y));
  }
  const auto scaled = image.scaled(QSize(42, 30), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QPixmap result(42, 30);
  result.fill(QColor(30, 32, 35));
  QPainter painter(&result);
  painter.drawImage(QPoint((result.width() - scaled.width()) / 2, (result.height() - scaled.height()) / 2), scaled);
  painter.setPen(QColor(92, 96, 102));
  painter.drawRect(result.rect().adjusted(0, 0, -1, -1));
  return result;
}

QPixmap sampled_grayscale_channel_thumbnail(const PixelBuffer& pixels) {
  if (pixels.empty() || pixels.format() != PixelFormat::gray8()) {
    return {};
  }
  const auto scaled_size =
      QSize(pixels.width(), pixels.height())
          .scaled(QSize(42, 30), Qt::KeepAspectRatio);
  QImage scaled(scaled_size, QImage::Format_Grayscale8);
  for (int y = 0; y < scaled.height(); ++y) {
    auto* destination = scaled.scanLine(y);
    const auto source_y = std::clamp(
        static_cast<int>((static_cast<std::int64_t>(y) * pixels.height()) /
                         std::max(1, scaled.height())),
        0, pixels.height() - 1);
    const auto source = pixels.row(source_y);
    for (int x = 0; x < scaled.width(); ++x) {
      const auto source_x = std::clamp(
          static_cast<int>((static_cast<std::int64_t>(x) * pixels.width()) /
                           std::max(1, scaled.width())),
          0, pixels.width() - 1);
      destination[x] = source[static_cast<std::size_t>(source_x)];
    }
  }
  QPixmap result(42, 30);
  result.fill(QColor(30, 32, 35));
  QPainter painter(&result);
  painter.drawImage(
      QPoint((result.width() - scaled.width()) / 2,
             (result.height() - scaled.height()) / 2),
      scaled);
  painter.setPen(QColor(92, 96, 102));
  painter.drawRect(result.rect().adjusted(0, 0, -1, -1));
  return result;
}

std::uint8_t white_backed_component(std::uint8_t component, std::uint8_t alpha) {
  return static_cast<std::uint8_t>((static_cast<int>(component) * static_cast<int>(alpha) +
                                    255 * (255 - static_cast<int>(alpha))) /
                                   255);
}

std::uint8_t channel_selection_luminance(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  return static_cast<std::uint8_t>((static_cast<int>(red) * 30 + static_cast<int>(green) * 59 +
                                    static_cast<int>(blue) * 11) /
                                   100);
}

}  // namespace

QPixmap MainWindow::cached_channel_thumbnail(const DocumentChannel& channel) {
  auto& cached = channel_thumbnail_cache_[channel.id()];
  if (cached.content_revision != channel.content_revision() || cached.thumbnail.isNull()) {
    cached.content_revision = channel.content_revision();
    cached.thumbnail = grayscale_channel_thumbnail(channel.pixels());
  }
  return cached.thumbnail;
}

void MainWindow::refresh_channel_panel() {
  // The Paths dock rides the same refresh cadence (document switches, undo
  // restores, mutations) so its rows never go stale relative to Channels.
  refresh_paths_panel();
  if (channel_panel_ == nullptr) {
    return;
  }
  if (!has_active_document() || canvas_ == nullptr) {
    channel_thumbnail_cache_.clear();
    quick_mask_thumbnail_canvas_ = nullptr;
    quick_mask_thumbnail_revision_ = 0;
    quick_mask_thumbnail_ = QPixmap();
    channel_panel_->set_rows({});
    channel_panel_->set_document_available(false);
    refresh_edit_target_chip();
    return;
  }

  const auto& doc = std::as_const(document());
  std::vector<ChannelPanel::Row> rows;
  rows.reserve(5U + doc.channels().size());
  rows.push_back({ChannelPanel::RowKind::Composite, 0, tr("Composite"),
                  component_channel_thumbnail(QColor(230, 230, 230)), false});
  rows.push_back({ChannelPanel::RowKind::Red, 0, tr("Red"), component_channel_thumbnail(QColor(235, 70, 70)),
                  false});
  rows.push_back({ChannelPanel::RowKind::Green, 0, tr("Green"),
                  component_channel_thumbnail(QColor(65, 210, 90)), false});
  rows.push_back({ChannelPanel::RowKind::Blue, 0, tr("Blue"),
                  component_channel_thumbnail(QColor(70, 120, 245)), false});
  // The CMYK proof's ink plates (docs/cmyk.md), while the canvas simulates the print
  // (View > Proof Colors, or the document is in CMYK Proof mode).
  if (const auto& proof = canvas_->proof_display_transform(); proof != nullptr && proof->simulates_colors()) {
    rows.push_back({ChannelPanel::RowKind::Cyan, 0, tr("Cyan"), ink_plate_thumbnail(QColor(0, 174, 239)), false});
    rows.push_back(
        {ChannelPanel::RowKind::Magenta, 0, tr("Magenta"), ink_plate_thumbnail(QColor(236, 0, 140)), false});
    rows.push_back({ChannelPanel::RowKind::Yellow, 0, tr("Yellow"), ink_plate_thumbnail(QColor(255, 242, 0)), false});
    rows.push_back({ChannelPanel::RowKind::Black, 0, tr("Black"), ink_plate_thumbnail(QColor(35, 31, 32)), false});
  }

  if (canvas_->quick_mask_active()) {
    if (quick_mask_thumbnail_canvas_ != canvas_ ||
        quick_mask_thumbnail_revision_ != canvas_->quick_mask_revision() ||
        quick_mask_thumbnail_.isNull()) {
      quick_mask_thumbnail_canvas_ = canvas_;
      quick_mask_thumbnail_revision_ = canvas_->quick_mask_revision();
      quick_mask_thumbnail_ =
          sampled_grayscale_channel_thumbnail(canvas_->quick_mask_pixels());
    }
    rows.push_back({ChannelPanel::RowKind::QuickMask, 0, tr("Quick Mask"),
                    quick_mask_thumbnail_, false});
  }

  std::set<ChannelId> current_ids;
  const auto selected_channel_id = canvas_->active_document_channel_id();
  for (const auto& channel : doc.channels()) {
    current_ids.insert(channel.id());
    const auto kind = channel.kind() == DocumentChannelKind::Spot ? ChannelPanel::RowKind::Spot
                                                                  : ChannelPanel::RowKind::Alpha;
    rows.push_back({kind, channel.id(), QString::fromStdString(channel.name()), cached_channel_thumbnail(channel),
                    selected_channel_id == channel.id() &&
                        canvas_->mask_display_mode() == CanvasWidget::MaskDisplayMode::Overlay});
  }
  for (auto it = channel_thumbnail_cache_.begin(); it != channel_thumbnail_cache_.end();) {
    if (!current_ids.contains(it->first)) {
      it = channel_thumbnail_cache_.erase(it);
    } else {
      ++it;
    }
  }

  ChannelPanel::Row selected = rows.front();
  const auto select_kind = [&rows, &selected](ChannelPanel::RowKind kind) {
    if (const auto found = std::find_if(rows.begin(), rows.end(),
                                        [kind](const ChannelPanel::Row& row) { return row.kind == kind; });
        found != rows.end()) {
      selected = *found;
    }
  };
  if (canvas_->quick_mask_active()) {
    select_kind(ChannelPanel::RowKind::QuickMask);
  } else {
    switch (canvas_->layer_edit_target()) {
      case CanvasWidget::LayerEditTarget::ComponentRed:
        select_kind(ChannelPanel::RowKind::Red);
        break;
      case CanvasWidget::LayerEditTarget::ComponentGreen:
        select_kind(ChannelPanel::RowKind::Green);
        break;
      case CanvasWidget::LayerEditTarget::ComponentBlue:
        select_kind(ChannelPanel::RowKind::Blue);
        break;
      case CanvasWidget::LayerEditTarget::ComponentCyan:
        select_kind(ChannelPanel::RowKind::Cyan);
        break;
      case CanvasWidget::LayerEditTarget::ComponentMagenta:
        select_kind(ChannelPanel::RowKind::Magenta);
        break;
      case CanvasWidget::LayerEditTarget::ComponentYellow:
        select_kind(ChannelPanel::RowKind::Yellow);
        break;
      case CanvasWidget::LayerEditTarget::ComponentBlack:
        select_kind(ChannelPanel::RowKind::Black);
        break;
      case CanvasWidget::LayerEditTarget::DocumentChannel:
        if (selected_channel_id.has_value()) {
          if (const auto found = std::find_if(
                  rows.begin(), rows.end(),
                  [&](const ChannelPanel::Row& row) {
                    return (row.kind == ChannelPanel::RowKind::Alpha || row.kind == ChannelPanel::RowKind::Spot) &&
                           row.id == *selected_channel_id;
                  });
              found != rows.end()) {
            selected = *found;
          }
        }
        break;
      case CanvasWidget::LayerEditTarget::Content:
      case CanvasWidget::LayerEditTarget::Mask:
      case CanvasWidget::LayerEditTarget::VectorMask:
      case CanvasWidget::LayerEditTarget::SmartFilterMask:
        break;
    }
  }
  channel_panel_->set_rows(std::move(rows), selected);
  const bool editing_available = !preview_dialog_edit_locked();
  channel_panel_->set_document_available(editing_available);
  channel_panel_->set_channel_creation_available(
      editing_available && doc.channels().size() < doc.maximum_saved_channel_count());
  refresh_edit_target_chip();
}

void MainWindow::set_channel_edit_target(ChannelPanel::RowKind kind, ChannelId id, bool overlay, bool announce) {
  if (canvas_ == nullptr || !has_active_document()) {
    return;
  }
  finish_active_text_editor();
  canvas_->finish_free_transform();
  canvas_->finish_warp_transform();
  if (canvas_->quick_mask_active() && kind != ChannelPanel::RowKind::QuickMask) {
    canvas_->set_quick_mask_active(false);
  }
  QString message;
  switch (kind) {
    case ChannelPanel::RowKind::Composite:
      set_layer_edit_target_ui(CanvasWidget::LayerEditTarget::Content, false);
      canvas_->set_mask_display_mode(CanvasWidget::MaskDisplayMode::None);
      message = tr("Showing composite");
      break;
    case ChannelPanel::RowKind::Red:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentRed);
      message = tr("Previewing Red channel (read-only)");
      break;
    case ChannelPanel::RowKind::Green:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentGreen);
      message = tr("Previewing Green channel (read-only)");
      break;
    case ChannelPanel::RowKind::Blue:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentBlue);
      message = tr("Previewing Blue channel (read-only)");
      break;
    case ChannelPanel::RowKind::Cyan:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentCyan);
      message = tr("Previewing Cyan plate (read-only)");
      break;
    case ChannelPanel::RowKind::Magenta:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentMagenta);
      message = tr("Previewing Magenta plate (read-only)");
      break;
    case ChannelPanel::RowKind::Yellow:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentYellow);
      message = tr("Previewing Yellow plate (read-only)");
      break;
    case ChannelPanel::RowKind::Black:
      canvas_->set_component_channel_preview(CanvasWidget::LayerEditTarget::ComponentBlack);
      message = tr("Previewing Black plate (read-only)");
      break;
    case ChannelPanel::RowKind::Alpha:
    case ChannelPanel::RowKind::Spot: {
      const auto* channel = static_cast<const Document&>(document()).find_channel(id);
      if (channel == nullptr) {
        set_layer_edit_target_ui(CanvasWidget::LayerEditTarget::Content, false);
        break;
      }
      canvas_->set_document_channel_edit_target(
          id, overlay ? CanvasWidget::MaskDisplayMode::Overlay : CanvasWidget::MaskDisplayMode::Grayscale);
      message = overlay ? tr("Showing channel overlay: %1").arg(QString::fromStdString(channel->name()))
                        : channel->kind() == DocumentChannelKind::Spot
                              ? tr("Previewing spot channel (read-only): %1").arg(QString::fromStdString(channel->name()))
                              : tr("Editing channel: %1").arg(QString::fromStdString(channel->name()));
      break;
    }
    case ChannelPanel::RowKind::QuickMask:
      if (!canvas_->quick_mask_active()) {
        canvas_->set_quick_mask_active(true);
      }
      message = tr("Editing Quick Mask");
      break;
  }
  refresh_layer_controls();
  refresh_options_bar();
  update_document_action_state();
  refresh_channel_panel();
  refresh_document_info();
  if (announce && !message.isEmpty()) {
    statusBar()->showMessage(message);
  }
}

DocumentChannel* MainWindow::selected_panel_channel() noexcept {
  if (!has_active_document() || channel_panel_ == nullptr) {
    return nullptr;
  }
  const auto selected = channel_panel_->selected_row();
  if (!selected.has_value() || (selected->kind != ChannelPanel::RowKind::Alpha &&
                                selected->kind != ChannelPanel::RowKind::Spot)) {
    return nullptr;
  }
  return document().find_channel(selected->id);
}

const DocumentChannel* MainWindow::selected_panel_channel() const noexcept {
  if (!has_active_document() || channel_panel_ == nullptr) {
    return nullptr;
  }
  const auto selected = channel_panel_->selected_row();
  if (!selected.has_value() || (selected->kind != ChannelPanel::RowKind::Alpha &&
                                selected->kind != ChannelPanel::RowKind::Spot)) {
    return nullptr;
  }
  return static_cast<const Document&>(document()).find_channel(selected->id);
}

void MainWindow::create_alpha_channel() {
  if (!has_active_document()) {
    return;
  }
  auto& doc = document();
  if (doc.channels().size() >= doc.maximum_saved_channel_count()) {
    show_status_error(tr("This document has reached the Photoshop channel limit"));
    return;
  }
  try {
    PixelBuffer pixels(doc.width(), doc.height(), PixelFormat::gray8());
    pixels.clear(0);
    const auto id = doc.allocate_channel_id();
    const auto name = doc.next_alpha_channel_name();
    push_undo_snapshot(tr("New channel"));
    doc.add_channel(DocumentChannel(id, name, DocumentChannelKind::Alpha,
                                    coverage_at_document_depth(doc, std::move(pixels))));
    set_channel_edit_target(ChannelPanel::RowKind::Alpha, id, false, false);
    statusBar()->showMessage(tr("Created channel %1").arg(QString::fromStdString(name)));
  } catch (const std::exception& error) {
    QMessageBox::warning(this, tr("New Channel"), translate_data_text(error.what()));
  }
}

void MainWindow::save_selection_as_channel() {
  if (!has_active_document() || canvas_ == nullptr) {
    return;
  }
  auto& doc = document();
  if (doc.channels().size() >= doc.maximum_saved_channel_count()) {
    show_status_error(tr("This document has reached the Photoshop channel limit"));
    return;
  }
  try {
    auto pixels = canvas_->selection_as_grayscale();
    const auto id = doc.allocate_channel_id();
    const auto name = doc.next_alpha_channel_name();
    push_undo_snapshot(tr("Save selection as channel"));
    doc.add_channel(DocumentChannel(id, name, DocumentChannelKind::Alpha,
                                    coverage_at_document_depth(doc, std::move(pixels))));
    set_channel_edit_target(ChannelPanel::RowKind::Alpha, id, false, false);
    statusBar()->showMessage(tr("Saved selection as %1").arg(QString::fromStdString(name)));
  } catch (const std::exception& error) {
    QMessageBox::warning(this, tr("Save Selection as Channel"), translate_data_text(error.what()));
  }
}

void MainWindow::load_channel_as_selection() {
  if (channel_panel_ == nullptr) {
    return;
  }
  const auto selected = channel_panel_->selected_row();
  if (!selected.has_value()) {
    return;
  }
  load_channel_as_selection(selected->kind, selected->id);
}

std::optional<MainWindow::ChannelRowPixels> MainWindow::channel_row_pixels(ChannelPanel::RowKind kind, ChannelId id,
                                                                         bool with_color) {
  if (canvas_ == nullptr || !has_active_document() || kind == ChannelPanel::RowKind::QuickMask) {
    return std::nullopt;
  }
  ChannelRowPixels result;
  if (kind == ChannelPanel::RowKind::Alpha || kind == ChannelPanel::RowKind::Spot) {
    const auto* channel = static_cast<const Document&>(document()).find_channel(id);
    if (channel == nullptr) {
      return std::nullopt;
    }
    // Selections and new layers take 8 bits: a 16/32-bit channel arrives narrowed.
    result.gray = channel->pixels().format().bit_depth != BitDepth::UInt8
                      ? convert_pixel_buffer_depth(channel->pixels(), BitDepth::UInt8, SampleKind::Coverage)
                      : channel->pixels();
    result.name = QString::fromStdString(channel->name());
    return result;
  }
  const auto& doc = static_cast<const Document&>(document());
  std::vector<std::uint8_t> merged_alpha;
  PixelBuffer flattened;
  if (doc.color_state().bit_depth == BitDepth::UInt8) {
    flattened = Compositor{}.flatten_rgb8(doc, &merged_alpha);
  } else {
    // 16/32-bit (docs/high-bit-depth.md): the deep composite at the selection's 8 bits.
    const auto rgba = convert_pixel_buffer_depth(Compositor{}.flatten_rgba_deep(doc), BitDepth::UInt8,
                                                 SampleKind::Color);
    flattened = PixelBuffer(doc.width(), doc.height(), PixelFormat::rgb8());
    merged_alpha.resize(static_cast<std::size_t>(doc.width()) * static_cast<std::size_t>(doc.height()));
    for (int y = 0; y < doc.height(); ++y) {
      for (int x = 0; x < doc.width(); ++x) {
        const auto* from = rgba.pixel(x, y);
        auto* to = flattened.pixel(x, y);
        to[0] = from[0];
        to[1] = from[1];
        to[2] = from[2];
        merged_alpha[static_cast<std::size_t>(y) * static_cast<std::size_t>(doc.width()) +
                     static_cast<std::size_t>(x)] = from[3];
      }
    }
  }
  result.gray = PixelBuffer(doc.width(), doc.height(), PixelFormat::gray8());
  // An ink plate's value is where it prints no ink (Photoshop's convention for a CMYK
  // channel), from the white-backed composite separated through the working space.
  std::vector<std::uint8_t> plate_inks;
  const auto space = working_cmyk_space();
  if (is_ink_plate_row(kind)) {
    std::vector<std::uint8_t> on_paper(static_cast<std::size_t>(doc.width()) * static_cast<std::size_t>(doc.height()) *
                                       4U);
    for (int y = 0; y < doc.height(); ++y) {
      const auto source = flattened.row(y);
      for (int x = 0; x < doc.width(); ++x) {
        const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(doc.width()) +
                           static_cast<std::size_t>(x);
        const auto* pixel = source.data() + static_cast<std::size_t>(x) * 3U;
        for (std::size_t channel = 0; channel < 3; ++channel) {
          on_paper[index * 4U + channel] = white_backed_component(pixel[channel], merged_alpha[index]);
        }
        on_paper[index * 4U + 3U] = 255;
      }
    }
    plate_inks = space->separate_rgba8(on_paper);
  }
  const std::size_t plate = kind == ChannelPanel::RowKind::Cyan      ? 0
                            : kind == ChannelPanel::RowKind::Magenta ? 1
                            : kind == ChannelPanel::RowKind::Yellow  ? 2
                                                                     : 3;
  // The colored form: a component in its own color over the composite's alpha (stacked in
  // Screen the three rebuild the image), or an ink plate as that ink alone prints on paper
  // (stacked in Multiply the four approximate the print).
  std::array<RgbColor, 256> ink_colors{};
  if (with_color && kind != ChannelPanel::RowKind::Composite) {
    result.color = PixelBuffer(doc.width(), doc.height(), PixelFormat::rgba8());
    if (is_ink_plate_row(kind)) {
      for (std::size_t level = 0; level < ink_colors.size(); ++level) {
        const auto percent = static_cast<double>(level) * 100.0 / 255.0;
        CmykPercent ink;
        (plate == 0 ? ink.cyan : plate == 1 ? ink.magenta : plate == 2 ? ink.yellow : ink.black) = percent;
        ink_colors[level] = space->rgb_from_cmyk(ink);
      }
    }
  }
  const std::size_t component = kind == ChannelPanel::RowKind::Red ? 0 : kind == ChannelPanel::RowKind::Green ? 1 : 2;
  for (int y = 0; y < doc.height(); ++y) {
    const auto source = flattened.row(y);
    auto destination = result.gray.row(y);
    std::uint8_t* colored = result.color.empty() ? nullptr : result.color.row(y).data();
    for (int x = 0; x < doc.width(); ++x) {
      const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(doc.width()) +
                         static_cast<std::size_t>(x);
      const auto* pixel = source.data() + static_cast<std::size_t>(x) * 3U;
      const auto red = white_backed_component(pixel[0], merged_alpha[index]);
      const auto green = white_backed_component(pixel[1], merged_alpha[index]);
      const auto blue = white_backed_component(pixel[2], merged_alpha[index]);
      auto& value = destination[static_cast<std::size_t>(x)];
      switch (kind) {
        case ChannelPanel::RowKind::Composite:
          value = channel_selection_luminance(red, green, blue);
          break;
        case ChannelPanel::RowKind::Red:
          value = red;
          break;
        case ChannelPanel::RowKind::Green:
          value = green;
          break;
        case ChannelPanel::RowKind::Blue:
          value = blue;
          break;
        case ChannelPanel::RowKind::Cyan:
        case ChannelPanel::RowKind::Magenta:
        case ChannelPanel::RowKind::Yellow:
        case ChannelPanel::RowKind::Black:
          value = static_cast<std::uint8_t>(255 - plate_inks[index * 4U + plate]);
          break;
        case ChannelPanel::RowKind::Alpha:
        case ChannelPanel::RowKind::Spot:
        case ChannelPanel::RowKind::QuickMask:
          break;
      }
      if (colored == nullptr) {
        continue;
      }
      auto* out = colored + static_cast<std::size_t>(x) * 4U;
      if (is_ink_plate_row(kind)) {
        const auto& ink = ink_colors[plate_inks[index * 4U + plate]];
        out[0] = ink.red;
        out[1] = ink.green;
        out[2] = ink.blue;
        out[3] = 255;
      } else {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        out[component] = pixel[component];
        out[3] = merged_alpha[index];
      }
    }
  }
  switch (kind) {
    case ChannelPanel::RowKind::Composite:
      result.name = tr("Composite");
      break;
    case ChannelPanel::RowKind::Red:
      result.name = tr("Red");
      break;
    case ChannelPanel::RowKind::Green:
      result.name = tr("Green");
      break;
    case ChannelPanel::RowKind::Blue:
      result.name = tr("Blue");
      break;
    case ChannelPanel::RowKind::Cyan:
      result.name = tr("Cyan");
      break;
    case ChannelPanel::RowKind::Magenta:
      result.name = tr("Magenta");
      break;
    case ChannelPanel::RowKind::Yellow:
      result.name = tr("Yellow");
      break;
    case ChannelPanel::RowKind::Black:
      result.name = tr("Black");
      break;
    case ChannelPanel::RowKind::Alpha:
    case ChannelPanel::RowKind::Spot:
    case ChannelPanel::RowKind::QuickMask:
      break;
  }
  return result;
}

void MainWindow::load_channel_as_selection(ChannelPanel::RowKind kind, ChannelId id) {
  const auto pixels = channel_row_pixels(kind, id, false);
  if (!pixels.has_value()) {
    return;
  }
  canvas_->replace_selection_from_grayscale(pixels->gray, tr("Load channel as selection"));
  statusBar()->showMessage(tr("Loaded %1 as selection").arg(pixels->name));
}

void MainWindow::new_layer_from_channel(bool in_color) {
  if (channel_panel_ == nullptr || canvas_ == nullptr || !has_active_document()) {
    return;
  }
  const auto selected = channel_panel_->selected_row();
  if (!selected.has_value()) {
    return;
  }
  auto pixels = channel_row_pixels(selected->kind, selected->id, in_color);
  if (!pixels.has_value() || (in_color && pixels->color.empty())) {
    return;
  }
  auto& doc = document();
  PixelBuffer layer_pixels;
  if (in_color) {
    layer_pixels = std::move(pixels->color);
  } else {
    layer_pixels = PixelBuffer(doc.width(), doc.height(), PixelFormat::rgba8());
    for (int y = 0; y < doc.height(); ++y) {
      const auto source = std::as_const(pixels->gray).row(y);
      auto* out = layer_pixels.row(y).data();
      for (std::size_t x = 0; x < static_cast<std::size_t>(doc.width()); ++x) {
        out[x * 4U] = source[x];
        out[x * 4U + 1U] = source[x];
        out[x * 4U + 2U] = source[x];
        out[x * 4U + 3U] = 255;
      }
    }
  }
  if (const auto depth = std::as_const(doc).color_state().bit_depth; depth != BitDepth::UInt8) {
    layer_pixels = convert_pixel_buffer_depth(layer_pixels, depth, SampleKind::Color);
  }
  const auto layer_name = in_color ? pixels->name : tr("%1 channel").arg(pixels->name);
  const auto blend_mode = !in_color                           ? BlendMode::Normal
                          : is_ink_plate_row(selected->kind) ? BlendMode::Multiply
                                                              : BlendMode::Screen;
  // A new layer is layer content: leave the channel view first, so the canvas shows it.
  set_channel_edit_target(ChannelPanel::RowKind::Composite, 0, false, false);
  auto anchor_id = doc.active_layer_id();
  if (const auto selected_ids = selected_layer_ids(); !selected_ids.empty()) {
    anchor_id = selected_ids.front();
  }
  push_undo_snapshot(in_color ? tr("New layer from channel color") : tr("New layer from channel"));
  Layer layer(doc.allocate_layer_id(), layer_name.toStdString(), std::move(layer_pixels));
  const auto layer_id = layer.id();
  layer.set_blend_mode(blend_mode);
  insert_layer_after_anchor(doc, std::move(layer), anchor_id);
  doc.set_active_layer(layer_id);
  refresh_layer_list();
  refresh_layer_controls();
  canvas_->document_changed();
  statusBar()->showMessage(tr("Created layer %1").arg(layer_name));
}

void MainWindow::rename_active_channel() {
  auto* channel = selected_panel_channel();
  if (channel == nullptr || channel->kind() != DocumentChannelKind::Alpha) {
    return;
  }
  bool accepted = false;
  const auto old_name = QString::fromStdString(channel->name());
  const auto name = QInputDialog::getText(this, tr("Rename Channel"), tr("Name:"), QLineEdit::Normal,
                                          old_name, &accepted).trimmed();
  if (!accepted || name.isEmpty() || name == old_name) {
    return;
  }
  const auto id = channel->id();
  push_undo_snapshot(tr("Rename channel"));
  document().rename_channel(id, name.toStdString());
  refresh_channel_panel();
  statusBar()->showMessage(tr("Renamed channel to %1").arg(name));
}

void MainWindow::invert_active_channel() {
  auto* channel = selected_panel_channel();
  if (channel == nullptr || channel->kind() != DocumentChannelKind::Alpha || canvas_ == nullptr) {
    return;
  }
  push_undo_snapshot(tr("Invert channel"));
  auto& pixels = channel->pixels();
  auto bytes = pixels.data();
  for (auto& value : bytes) {
    value = static_cast<std::uint8_t>(255U - value);
  }
  canvas_->grayscale_target_changed(QRect(0, 0, document().width(), document().height()));
  refresh_channel_panel();
  statusBar()->showMessage(tr("Inverted channel"));
}

void MainWindow::delete_active_channel() {
  auto* channel = selected_panel_channel();
  if (channel == nullptr || channel->kind() != DocumentChannelKind::Alpha) {
    return;
  }
  const auto id = channel->id();
  const auto name = QString::fromStdString(channel->name());
  push_undo_snapshot(tr("Delete channel"));
  document().remove_channel(id);
  set_channel_edit_target(ChannelPanel::RowKind::Composite, 0, false, false);
  statusBar()->showMessage(tr("Deleted channel %1").arg(name));
}

void MainWindow::reorder_channels_from_panel(std::vector<ChannelId> order) {
  if (!has_active_document()) {
    return;
  }
  auto& doc = document();
  const auto& current = std::as_const(doc).channels();
  if (order.size() != current.size()) {
    refresh_channel_panel();
    return;
  }
  std::set<ChannelId> unique(order.begin(), order.end());
  if (unique.size() != order.size()) {
    refresh_channel_panel();
    return;
  }
  bool changed = false;
  for (std::size_t index = 0; index < current.size(); ++index) {
    const auto* desired = static_cast<const Document&>(doc).find_channel(order[index]);
    if (desired == nullptr || (current[index].kind() == DocumentChannelKind::Spot && current[index].id() != order[index]) ||
        (desired->kind() == DocumentChannelKind::Spot && desired->id() != current[index].id())) {
      refresh_channel_panel();
      return;
    }
    changed = changed || current[index].id() != order[index];
  }
  if (!changed) {
    return;
  }
  push_undo_snapshot(tr("Reorder channels"));
  for (std::size_t index = 0; index < order.size(); ++index) {
    const auto& channels = static_cast<const Document&>(doc).channels();
    if (channels[index].id() != order[index]) {
      doc.reorder_channel(order[index], index);
    }
  }
  refresh_channel_panel();
  statusBar()->showMessage(tr("Reordered channels"));
}

void MainWindow::refresh_edit_target_chip() {
  if (mask_edit_mode_chip_ == nullptr || canvas_ == nullptr || !has_active_document()) {
    if (mask_edit_mode_chip_ != nullptr) {
      mask_edit_mode_chip_->hide();
    }
    return;
  }
  if (canvas_->quick_mask_active()) {
    mask_edit_mode_chip_->setText(tr("Editing Quick Mask (click to exit)"));
    mask_edit_mode_chip_->setToolTip(
        tr("Paint white to select, black to mask, or gray for partial selection."));
    mask_edit_mode_chip_->show();
    return;
  }
  if (canvas_->layer_edit_target() == CanvasWidget::LayerEditTarget::Mask) {
    mask_edit_mode_chip_->setText(tr("Editing layer mask (click to exit)"));
    mask_edit_mode_chip_->setToolTip(
        tr("Paint tools are editing the layer mask. Click to edit the layer pixels again."));
    mask_edit_mode_chip_->show();
    return;
  }
  if (canvas_->editing_smart_filter_mask()) {
    mask_edit_mode_chip_->setText(
        tr("Editing Smart Filter mask (click to exit)"));
    mask_edit_mode_chip_->setToolTip(
        tr("Paint tools are editing the shared Smart Filter mask. Click to edit the layer pixels again."));
    mask_edit_mode_chip_->show();
    return;
  }
  QString component_name;
  switch (canvas_->layer_edit_target()) {
    case CanvasWidget::LayerEditTarget::ComponentRed:
      component_name = tr("Red");
      break;
    case CanvasWidget::LayerEditTarget::ComponentGreen:
      component_name = tr("Green");
      break;
    case CanvasWidget::LayerEditTarget::ComponentBlue:
      component_name = tr("Blue");
      break;
    case CanvasWidget::LayerEditTarget::ComponentCyan:
      component_name = tr("Cyan");
      break;
    case CanvasWidget::LayerEditTarget::ComponentMagenta:
      component_name = tr("Magenta");
      break;
    case CanvasWidget::LayerEditTarget::ComponentYellow:
      component_name = tr("Yellow");
      break;
    case CanvasWidget::LayerEditTarget::ComponentBlack:
      component_name = tr("Black");
      break;
    case CanvasWidget::LayerEditTarget::Content:
    case CanvasWidget::LayerEditTarget::Mask:
    case CanvasWidget::LayerEditTarget::VectorMask:
    case CanvasWidget::LayerEditTarget::DocumentChannel:
    case CanvasWidget::LayerEditTarget::SmartFilterMask:
      break;
  }
  if (!component_name.isEmpty()) {
    mask_edit_mode_chip_->setText(tr("Viewing channel: %1 (click to exit)").arg(component_name));
    mask_edit_mode_chip_->setToolTip(tr("Click to return to the composite image."));
    mask_edit_mode_chip_->show();
    return;
  }
  if (canvas_->layer_edit_target() == CanvasWidget::LayerEditTarget::DocumentChannel) {
    const auto* channel = canvas_->active_document_channel_id().has_value()
                              ? static_cast<const Document&>(document()).find_channel(*canvas_->active_document_channel_id())
                              : nullptr;
    if (channel != nullptr) {
      mask_edit_mode_chip_->setText(
          channel->kind() == DocumentChannelKind::Spot
              ? tr("Viewing channel: %1 (click to exit)").arg(QString::fromStdString(channel->name()))
              : tr("Editing channel: %1 (click to exit)").arg(QString::fromStdString(channel->name())));
      mask_edit_mode_chip_->setToolTip(tr("Click to return to the composite image."));
      mask_edit_mode_chip_->show();
      return;
    }
  }
  mask_edit_mode_chip_->hide();
}

void MainWindow::restore_channel_target_after_document_reset(CanvasWidget::LayerEditTarget target,
                                                             std::optional<ChannelId> channel_id,
                                                             CanvasWidget::MaskDisplayMode display_mode) {
  if (canvas_ == nullptr || !has_active_document()) {
    return;
  }
  if (target == CanvasWidget::LayerEditTarget::DocumentChannel && channel_id.has_value() &&
      static_cast<const Document&>(document()).find_channel(*channel_id) != nullptr) {
    canvas_->set_document_channel_edit_target(*channel_id, display_mode);
  } else if (CanvasWidget::is_component_preview(target)) {
    canvas_->set_component_channel_preview(target);
  }
  refresh_channel_panel();
  update_document_action_state();
}

void MainWindow::toggle_quick_mask_mode() {
  if (canvas_ == nullptr || !has_active_document()) {
    return;
  }
  if (preview_dialog_edit_locked()) {
    show_preview_dialog_edit_lock_message();
    refresh_quick_mask_ui();
    return;
  }
  finish_active_text_editor();
  canvas_->finish_free_transform();
  canvas_->finish_warp_transform();
  const auto active = !canvas_->quick_mask_active();
  canvas_->set_quick_mask_active(active);
  refresh_quick_mask_ui();
  statusBar()->showMessage(active ? tr("Entered Quick Mask mode")
                                  : tr("Exited Quick Mask mode"));
}

void MainWindow::refresh_quick_mask_ui() {
  const auto active = canvas_ != nullptr && has_active_document() &&
                      canvas_->quick_mask_active();
  if (quick_mask_action_ != nullptr) {
    const QSignalBlocker blocker(quick_mask_action_);
    quick_mask_action_->setChecked(active);
  }
  refresh_color_buttons();
  refresh_channel_panel();
  update_document_action_state();
  refresh_document_info();
}

}  // namespace patchy::ui
