// The Object Selection tool and the shared automatic-selection combine
// (docs/ai-selection.md). The drag shows only a plain rectangle outline; MobileSAM solves
// once on release from one prompt, the dragged box or the clicked point, and the mask
// combines with the selection by plain boolean. Read the patent boundary in
// src/ai/segmentation_engine.hpp before changing how prompts or modes reach the network:
// no negative points, no box plus points in one solve, no previous mask fed back, and no
// hover preview (US 8050498 and Adobe's allowed hover-preview application).

#include "ui/canvas_widget.hpp"
#include "ui/canvas_widget_shared.hpp"

#include "ai/segmentation_engine.hpp"
#include "ui/background_workers.hpp"
#include "ui/image_document_io.hpp"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cstring>
#include <exception>
#include <utility>

namespace patchy::ui {
namespace {

// A release within this many widget pixels of the press is a click (point prompt).
constexpr int kObjectSelectClickSlop = 4;

// A 64-bit signature of the sampled pixels (RGBA8888 rows are 4-byte aligned, so the
// whole buffer is the pixels): decides whether a stale-marked embedding still matches.
std::uint64_t source_signature(const QImage& image) {
  std::uint64_t hash = 0x9E3779B97F4A7C15ULL ^ (static_cast<std::uint64_t>(image.width()) << 32U) ^
                       static_cast<std::uint64_t>(image.height());
  const auto* bytes = image.constBits();
  const auto size = static_cast<std::size_t>(image.sizeInBytes());
  std::size_t index = 0;
  for (; index + 8U <= size; index += 8U) {
    std::uint64_t word = 0;
    std::memcpy(&word, bytes + index, 8U);
    hash = (hash ^ word) * 0x100000001B3ULL;
    hash ^= hash >> 29U;
  }
  for (; index < size; ++index) {
    hash = (hash ^ bytes[index]) * 0x100000001B3ULL;
  }
  return hash;
}

QImage grayscale_image_from_mask(const patchy::ai::AiMask& mask) {
  QImage image(mask.bounds.width, mask.bounds.height, QImage::Format_Grayscale8);
  for (int y = 0; y < mask.bounds.height; ++y) {
    std::memcpy(image.scanLine(y), mask.alpha.row(y).data(), static_cast<std::size_t>(mask.bounds.width));
  }
  return image;
}

}  // namespace

void CanvasWidget::set_object_select_sample_all_layers(bool enabled) noexcept {
  if (object_select_sample_all_layers_ != enabled) {
    object_select_sample_all_layers_ = enabled;
    invalidate_object_select_embedding();
  }
}

bool CanvasWidget::object_select_sample_all_layers() const noexcept {
  return object_select_sample_all_layers_;
}

int CanvasWidget::object_select_encodes_started() const noexcept {
  return object_select_encodes_started_;
}

void CanvasWidget::invalidate_object_select_embedding() noexcept {
  // The worker owns its promise; dropping our shared_future never blocks.
  object_select_embedding_key_.reset();
  object_select_embedding_ = {};
  object_select_embedding_unverified_ = false;
}

void CanvasWidget::mark_object_select_embedding_unverified() noexcept {
  object_select_embedding_unverified_ = object_select_embedding_.valid();
}

void CanvasWidget::ensure_object_select_embedding() {
  if (document_ == nullptr || !patchy::ai::segmentation_available()) {
    return;
  }
  ObjectSelectEmbeddingKey key;
  key.sample_all_layers = object_select_sample_all_layers_;
  const auto cached = [this, &key] {
    return object_select_embedding_key_ == key && object_select_embedding_.valid();
  };
  QImage source;
  if (object_select_sample_all_layers_) {
    if (cached() && !object_select_embedding_unverified_) {
      return;
    }
    wait_for_move_commit_job();  // exact pixels: a deferred Move commit may still be rendering
    ensure_render_cache();
    source = render_cache_;
  } else {
    select_only_layer_if_none_active();
    const auto* layer = active_pixel_layer();
    if (layer == nullptr) {
      return;
    }
    key.layer = layer->id();
    if (cached() && !object_select_embedding_unverified_) {
      return;
    }
    source = active_layer_sample_image(*layer, QSize(document_->width(), document_->height()));
  }
  if (source.isNull()) {
    return;
  }
  const auto signature = source_signature(source);
  if (cached() && signature == object_select_source_hash_) {
    object_select_embedding_unverified_ = false;
    return;
  }
  // The encoder sees no prompt and nothing is displayed from it; it only prepares the
  // embedding the release-time solve decodes.
  auto promise = std::make_shared<std::promise<std::shared_ptr<const patchy::ai::SamEmbedding>>>();
  object_select_embedding_ = promise->get_future().share();
  object_select_embedding_key_ = key;
  object_select_source_hash_ = signature;
  object_select_embedding_unverified_ = false;
  ++object_select_encodes_started_;
  run_tracked_background_worker([promise, source = std::move(source)] {
    try {
      promise->set_value(patchy::ai::sam_encode(pixels_from_image_rgba(source)));
    } catch (...) {
      promise->set_exception(std::current_exception());
    }
  });
}

void CanvasWidget::begin_object_select(QPoint widget_position, QPoint document_point,
                                       Qt::KeyboardModifiers modifiers) {
  if (document_ == nullptr) {
    return;
  }
  if (!patchy::ai::segmentation_available()) {
    report_status_error(tr("Object Selection is not available in this build"));
    return;
  }
  if (!object_select_sample_all_layers_) {
    select_only_layer_if_none_active();
    if (active_pixel_layer() == nullptr) {
      report_status_error(tr("Select a pixel layer before using Object Selection"));
      return;
    }
  }
  auto operation = selection_operation(modifiers);
  if (operation == SelectionMode::Intersect) {
    // Like Quick Select there is no Intersect mode; Shift+Alt acts as Add.
    operation = SelectionMode::Add;
  }
  selection_operation_ = operation;
  object_selecting_ = true;
  object_select_press_widget_ = widget_position;
  object_select_release_widget_ = widget_position;
  object_select_press_ = document_point;
  object_select_current_ = document_point;
  ensure_object_select_embedding();
}

void CanvasWidget::extend_object_select(QPoint document_point) {
  if (!object_selecting_) {
    return;
  }
  if (document_point != object_select_current_) {
    object_select_current_ = document_point;
    update();
  }
}

void CanvasWidget::cancel_object_select() {
  if (object_selecting_) {
    object_selecting_ = false;
    update();
  }
}

// Runs the one release-time solve and commits it as one undo entry.
void CanvasWidget::finish_object_select(QPoint widget_position) {
  if (!object_selecting_) {
    return;
  }
  object_select_release_widget_ = widget_position;
  object_selecting_ = false;
  if (document_ == nullptr || !object_select_embedding_.valid()) {
    update();
    return;
  }
  const auto document_rect = QRect(0, 0, document_->width(), document_->height());
  const bool is_click = (object_select_release_widget_ - object_select_press_widget_).manhattanLength() <=
                        kObjectSelectClickSlop;
  const auto box = QRect(object_select_press_, object_select_current_).normalized().intersected(document_rect);
  if (!is_click && box.isEmpty()) {
    update();
    return;
  }

  auto embedding_future = object_select_embedding_;
  begin_processing_operation();
  set_processing_operation_message(tr("Analyzing image..."));
  wait_for_processing_operation(
      [&embedding_future] {
        return embedding_future.wait_for(std::chrono::milliseconds(16)) == std::future_status::ready;
      },
      true);
  patchy::ai::AiMask mask;
  QString failure;
  try {
    const auto embedding = embedding_future.get();
    if (is_click) {
      const auto point = object_select_press_;
      mask = patchy::ai::sam_select_point(*embedding, point.x() + 0.5, point.y() + 0.5);
    } else {
      mask = patchy::ai::sam_select_box(*embedding, Rect{box.x(), box.y(), box.width(), box.height()});
    }
  } catch (const std::exception& error) {
    failure = QString::fromUtf8(error.what());
    invalidate_object_select_embedding();
  }
  end_processing_operation();

  if (!failure.isEmpty()) {
    report_status_error(tr("Object Selection failed: %1").arg(failure));
    update();
    return;
  }
  if (mask.empty()) {
    if (status_callback_) {
      status_callback_(tr("Object Selection found no object there"));
    }
    update();
    return;
  }
  apply_automatic_selection_mask(QRect(mask.bounds.x, mask.bounds.y, mask.bounds.width, mask.bounds.height),
                                 grayscale_image_from_mask(mask), selection_operation_, tr("Object Selection"));
}

QImage CanvasWidget::merged_image_for_analysis() {
  return render_document_image_with_processing();
}

void CanvasWidget::apply_automatic_selection_mask(QRect bounds, QImage alpha, SelectionMode operation,
                                                  QString history_label) {
  if (document_ == nullptr || alpha.isNull() || bounds.isEmpty()) {
    return;
  }
  selection_edges_visible_ = true;
  capture_selection_before_edit();
  const auto previous_operation = selection_operation_;
  selection_operation_ = operation;
  if (!selection_antialias_) {
    for (int y = 0; y < alpha.height(); ++y) {
      auto* row = alpha.scanLine(y);
      for (int x = 0; x < alpha.width(); ++x) {
        row[x] = row[x] >= 128U ? 255U : 0U;
      }
    }
  }
  const auto document_rect = QRect(0, 0, document_->width(), document_->height());
  if (selection_feather_radius_ > 0) {
    const auto feather = selection_feather_radius_;
    const auto padding = feather_mask_padding(feather);
    const auto feather_bounds = bounds.adjusted(-padding, -padding, padding, padding).intersected(document_rect);
    QImage padded(feather_bounds.size(), QImage::Format_Grayscale8);
    padded.fill(0);
    QPainter painter(&padded);
    painter.drawImage(bounds.topLeft() - feather_bounds.topLeft(), alpha);
    painter.end();
    combine_selection_from_mask(feather_bounds, feather_blur_mask(std::move(padded), feather));
  } else {
    combine_selection_from_mask(bounds, std::move(alpha));
  }
  selection_operation_ = previous_operation;
  record_selection_history(std::move(history_label), selection_snapshot_before_edit());
  clear_selection_before_edit();
  refresh_info_display();
  update();
}

// The plain rectangle a box drag outlines. Only the user's input geometry is drawn; no
// segmentation runs or shows before release (see finish_object_select).
void CanvasWidget::draw_object_select_overlay(QPainter& painter) const {
  if (!object_selecting_ || object_select_press_ == object_select_current_) {
    return;
  }
  const auto rect = QRectF(widget_position_f(QPointF(object_select_press_)),
                           widget_position_f(QPointF(object_select_current_)))
                        .normalized();
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(Qt::black, 1.0, Qt::SolidLine));
  painter.drawRect(rect);
  QPen dashes(Qt::white, 1.0, Qt::CustomDashLine);
  dashes.setDashPattern({4.0, 4.0});
  painter.setPen(dashes);
  painter.drawRect(rect);
  painter.restore();
}

}  // namespace patchy::ui
