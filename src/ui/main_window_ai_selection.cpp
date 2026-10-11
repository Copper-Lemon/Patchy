// Select > Select Subject, Select > Select Background and Layer > Remove Background
// (docs/ai-selection.md). Each runs one BiRefNet pass over the merged document on a worker
// thread, under a cancellable progress dialog. The soft matte then combines with the
// selection, is inverted, or becomes a layer mask: plain booleans on the one result. One
// pass over the whole image only: no network-located crop and no second pass (patent
// boundary in src/ai/segmentation_engine.hpp).

#include "ui/main_window.hpp"

#include "ai/segmentation_engine.hpp"
#include "core/layer_render_utils.hpp"
#include "core/pixel_depth.hpp"
#include "core/rect_utils.hpp"
#include "ui/background_workers.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/app_settings.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/image_document_io.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/qt_geometry.hpp"
#include "ui/qt_paths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QLatin1String>
#include <QImage>
#include <QPointer>
#include <QProgressDialog>
#include <QStatusBar>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <future>
#include <optional>
#include <utility>

namespace patchy::ui {
namespace {

// How long the subject model stays loaded after the last subject command.
constexpr int kSubjectModelIdleReleaseMs = 3 * 60 * 1000;

QImage grayscale_image(const PixelBuffer& gray8) {
  QImage image(gray8.width(), gray8.height(), QImage::Format_Grayscale8);
  for (int y = 0; y < gray8.height(); ++y) {
    std::memcpy(image.scanLine(y), gray8.row(y).data(), static_cast<std::size_t>(gray8.width()));
  }
  return image;
}

// A painted mask plane's value at document (x, y) as 0..255, whatever its depth;
// default_color outside its bounds.
std::uint8_t painted_mask_value(const LayerMask& mask, std::int32_t x, std::int32_t y) {
  if (!mask.bounds.contains(x, y) || mask.pixels.empty()) {
    return mask.default_color;
  }
  const auto coverage = coverage_at(mask.pixels, x - mask.bounds.x, y - mask.bounds.y);
  return static_cast<std::uint8_t>(std::clamp(std::lround(coverage * 255.0F), 0L, 255L));
}

}  // namespace

void apply_subject_matte_to_layer_mask(const Document& document, Layer& layer, const patchy::ai::AiMask& matte) {
  // The new mask covers the subject; with an existing mask the two multiply, so anything
  // already hidden stays hidden and the mask keeps its density and feather settings.
  PixelBuffer coverage = matte.alpha;
  const std::optional<LayerMask> existing = std::as_const(layer).mask();
  if (existing.has_value()) {
    for (std::int32_t y = 0; y < matte.bounds.height; ++y) {
      auto row = coverage.row(y);
      for (std::int32_t x = 0; x < matte.bounds.width; ++x) {
        const auto old_value = painted_mask_value(*existing, matte.bounds.x + x, matte.bounds.y + y);
        auto& value = row[static_cast<std::size_t>(x)];
        value = static_cast<std::uint8_t>((static_cast<unsigned>(value) * old_value + 127U) / 255U);
      }
    }
  }
  LayerMask mask = existing.value_or(LayerMask{});
  mask.bounds = matte.bounds;
  mask.pixels = coverage_at_document_depth(document, std::move(coverage));
  mask.default_color = 0;
  layer.set_mask(std::move(mask));
}

void configure_automatic_selection_models() {
  if (!patchy::ai::model_directory().empty()) {
    return;
  }
  const QDir executable_directory(QCoreApplication::applicationDirPath());
  for (const auto* relative : {"ai-models", "../Resources/ai-models", "../share/patchy/ai-models"}) {
    const auto candidate = executable_directory.absoluteFilePath(QLatin1String(relative));
    if (QFileInfo(candidate).isDir()) {
      patchy::ai::set_model_directory(to_filesystem_path(QDir::cleanPath(candidate)));
      return;
    }
  }
}

std::optional<patchy::ai::AiMask> MainWindow::compute_subject_matte(const QString& command, QString* error) {
  const auto fail = [error](const QString& message) -> std::optional<patchy::ai::AiMask> {
    if (error != nullptr) {
      *error = message;
    }
    return std::nullopt;
  };
  if (canvas_ == nullptr) {
    return fail(tr("Open a document before using %1").arg(command));
  }
  if (!patchy::ai::segmentation_available()) {
    return fail(tr("%1 is not available in this build").arg(command));
  }
  if (canvas_->quick_mask_active()) {
    return fail(tr("%1 is unavailable in Quick Mask mode").arg(command));
  }
  const auto composite = canvas_->merged_image_for_analysis();
  if (composite.isNull()) {
    return fail(tr("%1 could not read the document").arg(command));
  }
  auto pixels = pixels_from_image_rgba(composite);

  if (subject_model_release_timer_ == nullptr) {
    subject_model_release_timer_ = new QTimer(this);
    subject_model_release_timer_->setSingleShot(true);
    subject_model_release_timer_->setInterval(kSubjectModelIdleReleaseMs);
    connect(subject_model_release_timer_, &QTimer::timeout, this, [] { patchy::ai::release_subject_model(); });
  }
  // The model must not be freed under the running worker.
  subject_model_release_timer_->stop();

  const QPointer<CanvasWidget> canvas = canvas_;
  patchy::ai::InferenceCancel cancel;
  auto future = launch_async([pixels = std::move(pixels), &cancel] { return patchy::ai::subject_matte(pixels, &cancel); });
  // The model reports no progress of its own: the percent is an estimate from how long the
  // last run took on this machine (persisted), held below 100 until the run really ends.
  auto settings = app_settings();
  const auto estimate_ms = std::clamp(
      settings.value(QStringLiteral("automaticSelection/subjectRunMs"), 7000).toLongLong(), 500LL, 120000LL);
  QElapsedTimer run_timer;
  run_timer.start();
  {
    QProgressDialog progress(tr("Finding the subject..."), tr("Cancel"), 0, 100, this);
    progress.setObjectName(QStringLiteral("selectSubjectProgressDialog"));
    progress.setWindowModality(Qt::WindowModal);
    // Shown at once (the run always takes seconds): the window-modal dialog is what keeps
    // the document from changing under the worker.
    progress.setMinimumDuration(0);
    remember_dialog_position(progress);
    connect(&progress, &QProgressDialog::canceled, this, [&cancel] { cancel.request(); });
    canvas->begin_processing_operation();
    QEventLoop wait_loop;
    QTimer poll;
    poll.setInterval(30);
    connect(&poll, &QTimer::timeout, &wait_loop, [&] {
      const auto percent = static_cast<int>(std::clamp<qint64>(run_timer.elapsed() * 100 / estimate_ms, 1, 95));
      progress.setValue(percent);
      if (canvas != nullptr) {
        canvas->set_processing_operation_message(tr("Finding the subject... %1%").arg(percent));
        canvas->tick_processing_operation();
      }
      if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        wait_loop.quit();
      }
    });
    if (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
      poll.start();
      wait_loop.exec();
    }
    if (canvas != nullptr) {
      canvas->end_processing_operation();
    }
  }
  subject_model_release_timer_->start();

  patchy::ai::AiMask matte;
  try {
    matte = future.get();
    // Blend the measured run into the stored estimate (load time included on purpose).
    settings.setValue(QStringLiteral("automaticSelection/subjectRunMs"),
                      static_cast<qlonglong>((estimate_ms + run_timer.elapsed()) / 2));
  } catch (const patchy::ai::InferenceCancelled&) {
    return fail(tr("%1 cancelled").arg(command));
  } catch (const std::exception& failure) {
    return fail(tr("%1 failed: %2").arg(command, QString::fromUtf8(failure.what())));
  }
  if (canvas == nullptr || canvas != canvas_) {
    return fail(tr("%1 cancelled").arg(command));
  }
  if (matte.empty()) {
    return fail(tr("%1 found no subject").arg(command));
  }
  return matte;
}

bool MainWindow::run_select_subject(CanvasWidget::SelectionMode mode, QString* error) {
  const auto matte = compute_subject_matte(tr("Select Subject"), error);
  if (!matte.has_value()) {
    return false;
  }
  canvas_->apply_automatic_selection_mask(
      QRect(matte->bounds.x, matte->bounds.y, matte->bounds.width, matte->bounds.height), grayscale_image(matte->alpha),
      mode, tr("Select Subject"));
  return true;
}

bool MainWindow::run_select_background(CanvasWidget::SelectionMode mode, QString* error) {
  const auto matte = compute_subject_matte(tr("Select Background"), error);
  if (!matte.has_value()) {
    return false;
  }
  // Everything the subject matte does not cover: 255 outside its bounds, inverted inside.
  const auto& doc = document();
  PixelBuffer background(doc.width(), doc.height(), PixelFormat::gray8());
  background.clear(255);
  for (std::int32_t y = 0; y < matte->bounds.height; ++y) {
    const auto source = matte->alpha.row(y);
    auto target = background.row(matte->bounds.y + y);
    for (std::int32_t x = 0; x < matte->bounds.width; ++x) {
      target[static_cast<std::size_t>(matte->bounds.x + x)] =
          static_cast<std::uint8_t>(255U - source[static_cast<std::size_t>(x)]);
    }
  }
  canvas_->apply_automatic_selection_mask(QRect(0, 0, doc.width(), doc.height()), grayscale_image(background), mode,
                                          tr("Select Background"));
  return true;
}

bool MainWindow::run_remove_background(QString* error) {
  const auto fail = [error](const QString& message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (canvas_ == nullptr) {
    return fail(tr("Open a document before using %1").arg(tr("Remove Background")));
  }
  auto& doc = document();
  select_only_layer_if_none_active();
  const auto active = doc.active_layer_id();
  const auto* candidate = active.has_value() ? std::as_const(doc).find_layer(*active) : nullptr;
  if (candidate == nullptr || (candidate->kind() != LayerKind::Pixel && candidate->kind() != LayerKind::Adjustment &&
                               candidate->kind() != LayerKind::Group)) {
    return fail(tr("Select a pixel, adjustment, or group layer before adding a mask"));
  }
  if (layer_id_locks_image_pixels(*active)) {
    return fail(tr("Layer pixels are locked."));
  }
  const auto matte = compute_subject_matte(tr("Remove Background"), error);
  if (!matte.has_value()) {
    return false;
  }
  auto* layer = doc.find_layer(*active);
  if (layer == nullptr) {
    return fail(tr("Select a pixel, adjustment, or group layer before adding a mask"));
  }

  push_undo_snapshot(tr("Remove Background"));
  const auto before = layer_render_bounds(*layer);
  apply_subject_matte_to_layer_mask(doc, *layer, *matte);
  const auto after = layer_render_bounds(*layer);
  canvas_->invalidate_mask_display();
  canvas_->document_changed(to_qrect(unite_rect(before, after)));
  refresh_layer_list();
  return true;
}

void MainWindow::select_subject(CanvasWidget::SelectionMode mode) {
  QString error;
  if (run_select_subject(mode, &error)) {
    statusBar()->showMessage(tr("Selected the subject"));
  } else if (!error.isEmpty()) {
    show_status_error(error);
  }
}

void MainWindow::select_background(CanvasWidget::SelectionMode mode) {
  QString error;
  if (run_select_background(mode, &error)) {
    statusBar()->showMessage(tr("Selected the background"));
  } else if (!error.isEmpty()) {
    show_status_error(error);
  }
}

void MainWindow::remove_background() {
  QString error;
  if (run_remove_background(&error)) {
    statusBar()->showMessage(tr("Removed the background with a layer mask"));
  } else if (!error.isEmpty()) {
    show_status_error(error);
  }
}

}  // namespace patchy::ui
