// Select > Subject (docs/ai-selection.md). One BiRefNet pass over the merged document on a
// worker thread, under a cancellable progress dialog; the soft matte combines with the
// selection by plain boolean. One pass over the whole image only: no network-located crop
// and no second pass (patent boundary in src/ai/segmentation_engine.hpp).

#include "ui/main_window.hpp"

#include "ai/segmentation_engine.hpp"
#include "ui/background_workers.hpp"
#include "ui/canvas_widget.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/image_document_io.hpp"
#include "ui/main_window_shared.hpp"
#include "ui/qt_paths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLatin1String>
#include <QImage>
#include <QPointer>
#include <QProgressDialog>
#include <QStatusBar>
#include <QTimer>

#include <chrono>
#include <cstring>
#include <exception>
#include <future>

namespace patchy::ui {
namespace {

// How long the subject model stays loaded after Select Subject finishes.
constexpr int kSubjectModelIdleReleaseMs = 3 * 60 * 1000;

}  // namespace

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

bool MainWindow::run_select_subject(CanvasWidget::SelectionMode mode, QString* error) {
  const auto fail = [error](const QString& message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (canvas_ == nullptr) {
    return fail(tr("Open a document before using Select Subject"));
  }
  if (!patchy::ai::segmentation_available()) {
    return fail(tr("Select Subject is not available in this build"));
  }
  if (canvas_->quick_mask_active()) {
    return fail(tr("Select Subject is unavailable in Quick Mask mode"));
  }
  const auto composite = canvas_->merged_image_for_analysis();
  if (composite.isNull()) {
    return fail(tr("Select Subject could not read the document"));
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
  {
    QProgressDialog progress(tr("Selecting the subject..."), tr("Cancel"), 0, 0, this);
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
      if (canvas != nullptr) {
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
  } catch (const patchy::ai::InferenceCancelled&) {
    return fail(tr("Select Subject cancelled"));
  } catch (const std::exception& failure) {
    return fail(tr("Select Subject failed: %1").arg(QString::fromUtf8(failure.what())));
  }
  if (canvas == nullptr || canvas != canvas_) {
    return fail(tr("Select Subject cancelled"));
  }
  if (matte.empty()) {
    return fail(tr("Select Subject found no subject"));
  }
  QImage alpha(matte.bounds.width, matte.bounds.height, QImage::Format_Grayscale8);
  for (int y = 0; y < matte.bounds.height; ++y) {
    std::memcpy(alpha.scanLine(y), matte.alpha.row(y).data(), static_cast<std::size_t>(matte.bounds.width));
  }
  canvas_->apply_automatic_selection_mask(QRect(matte.bounds.x, matte.bounds.y, matte.bounds.width, matte.bounds.height),
                                          std::move(alpha), mode, tr("Select Subject"));
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

}  // namespace patchy::ui
