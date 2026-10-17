// CanvasWidget's artboard surface (docs/artboards.md): the backdrop of an artboard document
// (checkerboard inside the artboards only, the pasteboard color between them), every
// artboard's outline and name label, and the Artboard tool. The tool lays out a new
// artboard by dragging on empty space, moves an artboard (with its content) by dragging it
// or its label, resizes it by its eight handles, and adds one beside it with the add
// buttons. Like the Crop tool it never edits the document: every gesture ends in an
// ArtboardEditRequest that MainWindow applies with an undo step.

#include "ui/canvas_widget.hpp"

#include "core/artboard.hpp"
#include "core/layer_tree.hpp"
#include "ui/canvas_types.hpp"
#include "ui/canvas_widget_shared.hpp"
#include "ui/theme_palette.hpp"

#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <Qt>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace patchy::ui {

namespace {

constexpr double kLabelGap = 4.0;          // widget px between the label baseline and the artboard
constexpr double kAddButtonRadius = 9.0;   // the "+" buttons beside the selected artboard
constexpr double kAddButtonOffset = 22.0;  // their centers, this far outside its edge
constexpr double kHandleSize = 8.0;

QRect qrect(const Rect& rect) {
  return QRect(rect.x, rect.y, rect.width, rect.height);
}

QFont artboard_label_font(const QWidget& widget) {
  QFont font = widget.font();
  font.setPointSizeF(std::max(7.0, font.pointSizeF() > 0.0 ? font.pointSizeF() - 0.5 : 8.5));
  return font;
}

}  // namespace

void CanvasWidget::set_artboard_edit_requested_callback(std::function<void(const ArtboardEditRequest&)> callback) {
  artboard_edit_requested_callback_ = std::move(callback);
}

void CanvasWidget::set_artboard_default_size(QSize size) noexcept {
  if (size.width() > 0 && size.height() > 0) {
    artboard_default_size_ = size;
  }
}

bool CanvasWidget::artboard_drag_active() const noexcept {
  return artboard_drag_mode_ != ArtboardDragMode::None;
}

void CanvasWidget::set_layers_moved_callback(std::function<void(const std::vector<LayerId>&)> callback) {
  layers_moved_callback_ = std::move(callback);
}

std::optional<LayerId> CanvasWidget::selected_root_artboard_id() const {
  if (document_ == nullptr) {
    return std::nullopt;
  }
  const auto& document = std::as_const(*document_);
  std::vector<LayerId> roots;
  if (!selected_layer_ids_.empty()) {
    roots = root_drop_layer_ids(document.layers(), selected_layer_ids_);
  } else if (const auto active = document.active_layer_id(); active.has_value()) {
    roots = {*active};
  }
  if (roots.size() != 1U) {
    return std::nullopt;
  }
  const auto* layer = document.find_layer(roots.front());
  return layer != nullptr && layer_is_artboard(*layer) ? std::optional<LayerId>(layer->id()) : std::nullopt;
}

void CanvasWidget::begin_artboard_move_drag(const Layer& artboard, QMouseEvent* event) {
  artboard_drag_mode_ = ArtboardDragMode::Move;
  artboard_drag_id_ = artboard.id();
  artboard_drag_start_rect_ = qrect(artboard.artboard()->rect);
  artboard_drag_rect_ = artboard_drag_start_rect_;
  artboard_drag_start_point_ = document_position_f(event->position());
  artboard_press_widget_point_ = event->pos();
  artboard_drag_moved_ = false;
  artboard_drag_duplicate_ = (event->modifiers() & Qt::AltModifier) != 0;
  artboard_drag_ghost_ = QImage();
}

bool CanvasWidget::handle_move_tool_artboard_press(QMouseEvent* event) {
  // With an artboard itself selected (not a layer inside it), the Move tool moves the
  // artboard: a press inside it starts the same gesture as the Artboard tool's.
  if (tool_ != CanvasTool::Move || event->button() != Qt::LeftButton || document_ == nullptr || edit_locked_ ||
      (event->modifiers() & Qt::ControlModifier) != 0) {
    return false;
  }
  const auto id = selected_root_artboard_id();
  if (!id.has_value()) {
    return false;
  }
  const auto* artboard = std::as_const(*document_).find_layer(*id);
  const auto point = document_position(event->pos());
  if (artboard == nullptr || !artboard->visible() || !artboard->artboard()->rect.contains(point.x(), point.y())) {
    return false;
  }
  begin_artboard_move_drag(*artboard, event);
  return true;
}

void CanvasWidget::request_artboard_edit(const ArtboardEditRequest& request) {
  if (artboard_edit_requested_callback_) {
    artboard_edit_requested_callback_(request);
  }
}

std::optional<LayerId> CanvasWidget::selected_artboard_id() const {
  if (document_ == nullptr) {
    return std::nullopt;
  }
  const auto& document = std::as_const(*document_);
  const auto active = document.active_layer_id();
  if (!active.has_value()) {
    return std::nullopt;
  }
  const auto* owner = artboard_owning_layer(document, *active);
  return owner != nullptr ? std::optional<LayerId>(owner->id()) : std::nullopt;
}

QRectF CanvasWidget::artboard_label_widget_rect(const Layer& artboard) const {
  const auto& rect = artboard.artboard()->rect;
  const QFontMetricsF metrics(artboard_label_font(*this));
  const auto top_left = widget_position_f(QPointF(rect.x, rect.y));
  const auto width = std::min(metrics.horizontalAdvance(QString::fromStdString(artboard.name())) + 2.0,
                              std::max(24.0, widget_position_f(QPointF(rect.x + rect.width, rect.y)).x() - top_left.x()));
  return QRectF(top_left.x(), top_left.y() - kLabelGap - metrics.height(), width, metrics.height());
}

std::optional<LayerId> CanvasWidget::artboard_label_at(QPoint widget_point) const {
  if (document_ == nullptr) {
    return std::nullopt;
  }
  for (const auto* artboard : document_artboards(std::as_const(*document_))) {
    if (artboard->visible() && artboard_label_widget_rect(*artboard).adjusted(-2.0, -2.0, 2.0, 2.0).contains(widget_point)) {
      return artboard->id();
    }
  }
  return std::nullopt;
}

QRectF CanvasWidget::artboard_add_button_widget_rect(QRect artboard_rect, int side) const {
  const auto widget = widget_rect_for_document_rect(QRectF(artboard_rect));
  QPointF center;
  switch (side) {
    case 1:
      center = QPointF(widget.center().x(), widget.bottom() + kAddButtonOffset);
      break;
    case 2:
      center = QPointF(widget.left() - kAddButtonOffset, widget.center().y());
      break;
    case 3:
      center = QPointF(widget.center().x(), widget.top() - kAddButtonOffset);
      break;
    default:
      center = QPointF(widget.right() + kAddButtonOffset, widget.center().y());
      break;
  }
  return QRectF(center.x() - kAddButtonRadius, center.y() - kAddButtonRadius, kAddButtonRadius * 2.0,
                kAddButtonRadius * 2.0);
}

int CanvasWidget::artboard_add_button_at(QPoint widget_point) const {
  if (tool_ != CanvasTool::Artboard || document_ == nullptr || artboard_drag_active()) {
    return -1;
  }
  const auto id = selected_artboard_id();
  const auto* artboard = id.has_value() ? std::as_const(*document_).find_layer(*id) : nullptr;
  if (artboard == nullptr) {
    return -1;
  }
  for (int side = 0; side < 4; ++side) {
    // The top button would sit on the label; the label wins there.
    if (side == 3 && artboard_label_widget_rect(*artboard).contains(widget_point)) {
      continue;
    }
    if (artboard_add_button_widget_rect(qrect(artboard->artboard()->rect), side).contains(widget_point)) {
      return side;
    }
  }
  return -1;
}

CanvasWidget::TransformHandle CanvasWidget::artboard_handle_at(QPoint widget_point) const {
  if (tool_ != CanvasTool::Artboard || document_ == nullptr) {
    return TransformHandle::None;
  }
  const auto id = selected_artboard_id();
  const auto* artboard = id.has_value() ? std::as_const(*document_).find_layer(*id) : nullptr;
  if (artboard == nullptr) {
    return TransformHandle::None;
  }
  const auto handle = transform_handle_at(widget_point, QRectF(qrect(artboard->artboard()->rect)), 0.0);
  return handle == TransformHandle::Rotate || handle == TransformHandle::Move ? TransformHandle::None : handle;
}

QRect CanvasWidget::artboard_create_rect(QPoint anchor, QPoint current, Qt::KeyboardModifiers modifiers) const {
  auto delta = current - anchor;
  if ((modifiers & Qt::ShiftModifier) != 0) {
    const auto side = std::max(std::abs(delta.x()), std::abs(delta.y()));
    delta = QPoint(delta.x() < 0 ? -side : side, delta.y() < 0 ? -side : side);
  }
  if ((modifiers & Qt::AltModifier) != 0) {
    // Alt draws from the center, like the Crop tool and the marquees.
    const auto half_w = std::max(1, std::abs(delta.x()));
    const auto half_h = std::max(1, std::abs(delta.y()));
    return QRect(anchor.x() - half_w, anchor.y() - half_h, half_w * 2, half_h * 2);
  }
  // Edges, not pixels: from x 20 to x 120 is 100 px (QRect(a, b) would count both ends).
  const auto end = anchor + delta;
  return QRect(std::min(anchor.x(), end.x()), std::min(anchor.y(), end.y()), std::max(1, std::abs(delta.x())),
               std::max(1, std::abs(delta.y())));
}

QRect CanvasWidget::artboard_resize_rect(QPoint point, Qt::KeyboardModifiers modifiers) const {
  const auto start = artboard_drag_start_rect_;
  const auto handle = artboard_drag_handle_;
  const bool moves_left = handle == TransformHandle::TopLeft || handle == TransformHandle::Left ||
                          handle == TransformHandle::BottomLeft;
  const bool moves_right = handle == TransformHandle::TopRight || handle == TransformHandle::Right ||
                           handle == TransformHandle::BottomRight;
  const bool moves_top = handle == TransformHandle::TopLeft || handle == TransformHandle::Top ||
                         handle == TransformHandle::TopRight;
  const bool moves_bottom = handle == TransformHandle::BottomLeft || handle == TransformHandle::Bottom ||
                            handle == TransformHandle::BottomRight;
  const bool corner = (moves_left || moves_right) && (moves_top || moves_bottom);
  const double ratio = corner && (modifiers & Qt::ShiftModifier) != 0 && start.height() > 0
                           ? static_cast<double>(start.width()) / start.height()
                           : 0.0;
  if ((modifiers & Qt::AltModifier) != 0) {
    return center_anchored_handle_rect(start, moves_left || moves_right, moves_top || moves_bottom, point, ratio);
  }
  auto left = start.x();
  auto top = start.y();
  auto right = start.x() + start.width();
  auto bottom = start.y() + start.height();
  if (moves_left) {
    left = point.x();
  }
  if (moves_right) {
    right = point.x();
  }
  if (moves_top) {
    top = point.y();
  }
  if (moves_bottom) {
    bottom = point.y();
  }
  QRect rect(std::min(left, right), std::min(top, bottom), std::max(1, std::abs(right - left)),
             std::max(1, std::abs(bottom - top)));
  if (ratio > 0.0) {
    // Shift on a corner keeps the starting aspect: the larger change wins.
    const auto anchor_x = moves_left ? start.x() + start.width() : start.x();
    const auto anchor_y = moves_top ? start.y() + start.height() : start.y();
    auto width = rect.width();
    auto height = rect.height();
    if (static_cast<double>(width) / height > ratio) {
      height = std::max(1, static_cast<int>(std::lround(width / ratio)));
    } else {
      width = std::max(1, static_cast<int>(std::lround(height * ratio)));
    }
    rect = QRect(point.x() < anchor_x ? anchor_x - width : anchor_x, point.y() < anchor_y ? anchor_y - height : anchor_y,
                 width, height);
  }
  return rect;
}

bool CanvasWidget::handle_artboard_label_press(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || document_ == nullptr || edit_locked_) {
    return false;
  }
  const auto label = artboard_label_at(event->pos());
  if (!label.has_value()) {
    return false;
  }
  request_artboard_edit({ArtboardEditRequest::Kind::Select, label, {}, {}, 0, false});
  // A press on the label also starts a move of the whole artboard, with any tool that
  // shows labels (Move and Artboard), as in Photoshop.
  if (tool_ == CanvasTool::Move || tool_ == CanvasTool::Artboard) {
    const auto* artboard = std::as_const(*document_).find_layer(*label);
    if (artboard != nullptr && artboard->artboard().has_value()) {
      begin_artboard_move_drag(*artboard, event);
    }
  }
  return true;
}

bool CanvasWidget::handle_artboard_label_double_click(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || document_ == nullptr || edit_locked_) {
    return false;
  }
  const auto label = artboard_label_at(event->pos());
  if (!label.has_value()) {
    return false;
  }
  cancel_artboard_drag();
  request_artboard_edit({ArtboardEditRequest::Kind::Rename, label, {}, {}, 0, false});
  return true;
}

bool CanvasWidget::handle_artboard_press(QMouseEvent* event) {
  if (tool_ != CanvasTool::Artboard || event->button() != Qt::LeftButton || document_ == nullptr) {
    return false;
  }
  if (edit_locked_) {
    return true;  // the tool owns the press; nothing may change
  }
  const auto& document = std::as_const(*document_);
  artboard_press_widget_point_ = event->pos();
  artboard_drag_start_point_ = document_position_f(event->position());
  artboard_drag_moved_ = false;
  artboard_drag_duplicate_ = false;
  artboard_drag_ghost_ = QImage();

  // The add buttons, then the selected artboard's handles, then any label.
  if (const auto side = artboard_add_button_at(event->pos()); side >= 0) {
    request_artboard_edit({ArtboardEditRequest::Kind::AddAdjacent, selected_artboard_id(), {}, {}, side, false});
    return true;
  }
  if (const auto handle = artboard_handle_at(event->pos()); handle != TransformHandle::None) {
    const auto id = selected_artboard_id();
    artboard_drag_mode_ = ArtboardDragMode::Resize;
    artboard_drag_id_ = id;
    artboard_drag_handle_ = handle;
    artboard_drag_start_rect_ = qrect(document.find_layer(*id)->artboard()->rect);
    artboard_drag_rect_ = artboard_drag_start_rect_;
    return true;
  }
  if (handle_artboard_label_press(event)) {
    return true;
  }
  // Inside an artboard: select it, and a drag moves it with its content (Alt copies it).
  const auto point = document_position(event->pos());
  if (const auto* artboard = artboard_at(document, point.x(), point.y()); artboard != nullptr) {
    request_artboard_edit({ArtboardEditRequest::Kind::Select, artboard->id(), {}, {}, 0, false});
    artboard_drag_mode_ = ArtboardDragMode::Move;
    artboard_drag_id_ = artboard->id();
    artboard_drag_start_rect_ = qrect(artboard->artboard()->rect);
    artboard_drag_rect_ = artboard_drag_start_rect_;
    artboard_drag_duplicate_ = (event->modifiers() & Qt::AltModifier) != 0;
    return true;
  }
  // Empty space (pasteboard, gaps, loose content): lay out a new artboard.
  artboard_drag_mode_ = ArtboardDragMode::Create;
  artboard_drag_id_.reset();
  artboard_drag_anchor_document_ = snapped_document_point(point);
  artboard_drag_rect_ = QRect(artboard_drag_anchor_document_, QSize(1, 1));
  return true;
}

void CanvasWidget::update_artboard_drag(QMouseEvent* event) {
  if (artboard_drag_mode_ == ArtboardDragMode::None) {
    return;
  }
  if (!artboard_drag_moved_ &&
      (event->pos() - artboard_press_widget_point_).manhattanLength() >= QApplication::startDragDistance()) {
    artboard_drag_moved_ = true;
    if (artboard_drag_mode_ == ArtboardDragMode::Move && document_ != nullptr) {
      // The artboard as it looks now, carried along at the moving rect.
      const auto rect = artboard_drag_start_rect_.intersected(QRect(0, 0, document_->width(), document_->height()));
      if (!render_cache_.isNull() && render_cache_.size() == QSize(document_->width(), document_->height()) &&
          !rect.isEmpty()) {
        artboard_drag_ghost_ = render_cache_.copy(rect);
      }
    }
  }
  if (!artboard_drag_moved_) {
    return;
  }
  const auto point = document_position_f(event->position());
  // The pixel under the pointer, as the anchor was taken: a drag from pixel 20 to pixel
  // 120 is 100 px wide.
  const auto pixel = document_position(event->pos());
  switch (artboard_drag_mode_) {
    case ArtboardDragMode::Create:
      artboard_drag_rect_ =
          artboard_create_rect(artboard_drag_anchor_document_, snapped_document_point(pixel), event->modifiers());
      break;
    case ArtboardDragMode::Move: {
      const QPoint raw_delta(static_cast<int>(std::lround(point.x() - artboard_drag_start_point_.x())),
                             static_cast<int>(std::lround(point.y() - artboard_drag_start_point_.y())));
      auto delta = snapped_rect_delta(artboard_drag_start_rect_, raw_delta);
      if ((event->modifiers() & Qt::ShiftModifier) != 0) {
        // Shift keeps the move on one axis.
        if (std::abs(delta.x()) >= std::abs(delta.y())) {
          delta.setY(0);
        } else {
          delta.setX(0);
        }
      }
      artboard_drag_rect_ = artboard_drag_start_rect_.translated(delta);
      artboard_drag_duplicate_ = artboard_drag_duplicate_ || (event->modifiers() & Qt::AltModifier) != 0;
      break;
    }
    case ArtboardDragMode::Resize:
      artboard_drag_rect_ = artboard_resize_rect(snapped_document_point(pixel), event->modifiers());
      break;
    case ArtboardDragMode::None:
      break;
  }
  update();
}

void CanvasWidget::finish_artboard_drag(QMouseEvent* event) {
  if (artboard_drag_mode_ == ArtboardDragMode::None) {
    return;
  }
  update_artboard_drag(event);
  const auto mode = artboard_drag_mode_;
  const auto id = artboard_drag_id_;
  const auto moved = artboard_drag_moved_;
  const auto rect = artboard_drag_rect_;
  const QRect start = artboard_drag_start_rect_;
  const auto duplicate = artboard_drag_duplicate_;
  cancel_artboard_drag();
  if (!moved) {
    return;  // a click: the press already selected what it hit
  }
  switch (mode) {
    case ArtboardDragMode::Create:
      if (rect.width() >= 2 && rect.height() >= 2) {
        request_artboard_edit({ArtboardEditRequest::Kind::Create, std::nullopt, rect, {}, 0, false});
      }
      break;
    case ArtboardDragMode::Move:
      if (rect.topLeft() != start.topLeft() || duplicate) {
        request_artboard_edit(
            {ArtboardEditRequest::Kind::Move, id, rect, rect.topLeft() - start.topLeft(), 0, duplicate});
      }
      break;
    case ArtboardDragMode::Resize:
      if (rect != start) {
        request_artboard_edit({ArtboardEditRequest::Kind::Resize, id, rect, {}, 0, false});
      }
      break;
    case ArtboardDragMode::None:
      break;
  }
}

void CanvasWidget::cancel_artboard_drag() {
  const bool had_drag = artboard_drag_mode_ != ArtboardDragMode::None;
  artboard_drag_mode_ = ArtboardDragMode::None;
  artboard_drag_id_.reset();
  artboard_drag_handle_ = TransformHandle::None;
  artboard_drag_moved_ = false;
  artboard_drag_duplicate_ = false;
  artboard_drag_ghost_ = QImage();
  if (had_drag) {
    update();
  }
}

bool CanvasWidget::nudge_selected_artboard(QPoint delta) {
  if ((tool_ != CanvasTool::Artboard && tool_ != CanvasTool::Move) || delta.isNull() || edit_locked_ ||
      artboard_drag_active()) {
    return false;
  }
  // The Artboard tool nudges the artboard of whatever is selected; the Move tool only an
  // artboard selected itself (a layer inside one nudges as a layer).
  const auto id = tool_ == CanvasTool::Artboard ? selected_artboard_id() : selected_root_artboard_id();
  if (!id.has_value()) {
    return false;
  }
  const auto* artboard = std::as_const(*document_).find_layer(*id);
  const auto& rect = artboard->artboard()->rect;
  request_artboard_edit({ArtboardEditRequest::Kind::Move, id,
                         QRect(rect.x + delta.x(), rect.y + delta.y(), rect.width, rect.height), delta, 0, false,
                         true});
  return true;
}

void CanvasWidget::update_artboard_tool_cursor(QPoint widget_point) {
  if (tool_ != CanvasTool::Artboard || document_ == nullptr) {
    return;
  }
  if (artboard_drag_mode_ == ArtboardDragMode::Move) {
    setCursor(Qt::ClosedHandCursor);
    return;
  }
  if (artboard_add_button_at(widget_point) >= 0) {
    setCursor(Qt::PointingHandCursor);
    return;
  }
  switch (artboard_drag_mode_ == ArtboardDragMode::Resize ? artboard_drag_handle_ : artboard_handle_at(widget_point)) {
    case TransformHandle::TopLeft:
    case TransformHandle::BottomRight:
      setCursor(Qt::SizeFDiagCursor);
      return;
    case TransformHandle::TopRight:
    case TransformHandle::BottomLeft:
      setCursor(Qt::SizeBDiagCursor);
      return;
    case TransformHandle::Left:
    case TransformHandle::Right:
      setCursor(Qt::SizeHorCursor);
      return;
    case TransformHandle::Top:
    case TransformHandle::Bottom:
      setCursor(Qt::SizeVerCursor);
      return;
    default:
      break;
  }
  const auto point = document_position(widget_point);
  if (artboard_label_at(widget_point).has_value() || artboard_at(std::as_const(*document_), point.x(), point.y()) != nullptr) {
    setCursor(Qt::OpenHandCursor);
    return;
  }
  setCursor(Qt::CrossCursor);
}

void CanvasWidget::draw_canvas_backdrop(QPainter& painter, const QRectF& target_rect, QRect exposed_rect) const {
  if (document_ == nullptr || !document_has_artboards(std::as_const(*document_))) {
    draw_checkerboard(painter, target_rect, exposed_rect);
    return;
  }
  // An artboard document: between the artboards is pasteboard (the composite there is
  // transparent, and only loose layers paint it); inside each artboard the usual
  // checkerboard shows what a Transparent background lets through.
  painter.fillRect(target_rect.intersected(QRectF(exposed_rect)), backdrop_color());
  for (const auto* artboard : document_artboards(std::as_const(*document_))) {
    if (!artboard->visible()) {
      continue;
    }
    const auto& rect = artboard->artboard()->rect;
    draw_checkerboard(painter, widget_rect_for_document_rect(QRectF(rect.x, rect.y, rect.width, rect.height)).intersected(target_rect),
                      exposed_rect);
  }
}

void CanvasWidget::draw_artboard_overlay(QPainter& painter) const {
  if (document_ == nullptr) {
    return;
  }
  const auto& document = std::as_const(*document_);
  const auto artboards = document_artboards(document);
  const auto selected = selected_artboard_id();
  const auto& colors = theme();
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  const auto font = artboard_label_font(*this);
  painter.setFont(font);
  const QFontMetricsF metrics(font);

  // The moving artboard's pixels at the drag rect (the original stays until release).
  if (artboard_drag_mode_ == ArtboardDragMode::Move && artboard_drag_moved_ && !artboard_drag_ghost_.isNull()) {
    const auto clipped = artboard_drag_start_rect_.intersected(QRect(0, 0, document_->width(), document_->height()));
    const auto offset = artboard_drag_rect_.topLeft() - artboard_drag_start_rect_.topLeft();
    painter.setOpacity(0.8);
    painter.drawImage(widget_rect_for_document_rect(QRectF(clipped.translated(offset))), artboard_drag_ghost_);
    painter.setOpacity(1.0);
  }

  for (const auto* artboard : artboards) {
    if (!artboard->visible()) {
      continue;
    }
    auto rect = artboard->artboard()->rect;
    const bool is_selected = selected.has_value() && *selected == artboard->id();
    const bool is_dragged = artboard_drag_active() && artboard_drag_moved_ && artboard_drag_id_ == artboard->id() &&
                            artboard_drag_mode_ != ArtboardDragMode::Create;
    if (is_dragged && !artboard_drag_duplicate_) {
      rect = Rect{artboard_drag_rect_.x(), artboard_drag_rect_.y(), artboard_drag_rect_.width(),
                  artboard_drag_rect_.height()};
    }
    const auto widget_rect = widget_rect_for_document_rect(QRectF(rect.x, rect.y, rect.width, rect.height));
    QPen outline(is_selected ? colors.canvas_artboard_selected : colors.canvas_artboard_outline, is_selected ? 2.0 : 1.0);
    outline.setCosmetic(true);
    painter.setPen(outline);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(widget_rect.adjusted(0.5, 0.5, -0.5, -0.5));
    // The name, above the top-left corner and elided to the artboard's width.
    const auto label = artboard_label_widget_rect(*artboard).translated(widget_rect.topLeft() -
                                                                        widget_position_f(QPointF(artboard->artboard()->rect.x,
                                                                                                  artboard->artboard()->rect.y)));
    painter.setPen(is_selected ? colors.canvas_artboard_selected : colors.canvas_artboard_label);
    painter.drawText(label, Qt::AlignLeft | Qt::AlignVCenter,
                     metrics.elidedText(QString::fromStdString(artboard->name()), Qt::ElideRight, label.width()));
    if (is_dragged && artboard_drag_duplicate_) {
      // An Alt-drag copies: the copy's outline follows the pointer.
      QPen copy_pen(colors.canvas_artboard_selected, 1.0, Qt::DashLine);
      copy_pen.setCosmetic(true);
      painter.setPen(copy_pen);
      painter.drawRect(widget_rect_for_document_rect(QRectF(artboard_drag_rect_)).adjusted(0.5, 0.5, -0.5, -0.5));
    }
  }

  if (tool_ == CanvasTool::Artboard) {
    if (artboard_drag_mode_ == ArtboardDragMode::Create && artboard_drag_moved_) {
      QPen pen(colors.canvas_artboard_selected, 1.0, Qt::DashLine);
      pen.setCosmetic(true);
      painter.setPen(pen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(widget_rect_for_document_rect(QRectF(artboard_drag_rect_)).adjusted(0.5, 0.5, -0.5, -0.5));
    }
    const auto* artboard = selected.has_value() ? document.find_layer(*selected) : nullptr;
    if (artboard != nullptr && artboard->visible() && artboard_drag_mode_ != ArtboardDragMode::Move) {
      const auto rect = artboard_drag_mode_ == ArtboardDragMode::Resize
                            ? QRectF(artboard_drag_rect_)
                            : QRectF(artboard->artboard()->rect.x, artboard->artboard()->rect.y,
                                     artboard->artboard()->rect.width, artboard->artboard()->rect.height);
      if (artboard_drag_mode_ == ArtboardDragMode::Resize) {
        QPen pen(colors.canvas_artboard_selected, 1.0, Qt::DashLine);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(widget_rect_for_document_rect(rect).adjusted(0.5, 0.5, -0.5, -0.5));
      }
      painter.setPen(QPen(colors.canvas_artboard_selected, 1.0));
      painter.setBrush(colors.canvas_artboard_handle);
      for (const auto handle : {TransformHandle::TopLeft, TransformHandle::Top, TransformHandle::TopRight,
                                TransformHandle::Right, TransformHandle::BottomRight, TransformHandle::Bottom,
                                TransformHandle::BottomLeft, TransformHandle::Left}) {
        const auto point = transform_handle_position(handle, rect, 0.0);
        painter.drawRect(QRectF(point.x() - kHandleSize / 2.0, point.y() - kHandleSize / 2.0, kHandleSize, kHandleSize));
      }
      if (!artboard_drag_active()) {
        // Add buttons: a "+" in a circle beside each edge adds an artboard of this size there.
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int side = 0; side < 4; ++side) {
          const auto button = artboard_add_button_widget_rect(qrect(artboard->artboard()->rect), side);
          painter.setPen(QPen(colors.canvas_artboard_selected, 1.2));
          painter.setBrush(colors.canvas_hud_bg);
          painter.drawEllipse(button);
          painter.setPen(QPen(colors.canvas_hud_text, 1.6, Qt::SolidLine, Qt::RoundCap));
          const auto center = button.center();
          painter.drawLine(QPointF(center.x() - 4.0, center.y()), QPointF(center.x() + 4.0, center.y()));
          painter.drawLine(QPointF(center.x(), center.y() - 4.0), QPointF(center.x(), center.y() + 4.0));
        }
      }
    }
  }
  painter.restore();
}

}  // namespace patchy::ui
