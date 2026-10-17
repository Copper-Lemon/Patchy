#include "ui/print_dialog.hpp"

#include "ui/app_settings.hpp"
#include "ui/image_document_io.hpp"
#include "ui/pdf_export.hpp"
#include "ui/print_internal.hpp"

#include <QColor>
#include <QFileInfo>
#include <QMarginsF>
#include <QObject>
#include <QPageSize>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

// The portable half of the print code: page layout defaults, placement math,
// and page rendering. Compiled on every platform, including wasm, where the
// QtPrintSupport dialog TU (print_dialog.cpp) is replaced by stubs.
namespace patchy::ui {

namespace {

using print_detail::document_horizontal_ppi;
using print_detail::document_vertical_ppi;
using print_detail::valid_page_layout;

constexpr double kPointsPerInch = 72.0;

double sanitized_scale_percent(double value) noexcept {
  return std::clamp(std::isfinite(value) ? value : 100.0, 1.0, 1000.0);
}

QRect document_rect(const Document& document) {
  return QRect(0, 0, std::max<std::int32_t>(0, document.width()), std::max<std::int32_t>(0, document.height()));
}

QRect source_rect_for_settings(const Document& document, const PrintSettings& settings) {
  const auto full = document_rect(document);
  if (settings.area_mode != PrintAreaMode::Selection || settings.selection_bounds.isEmpty()) {
    return full;
  }
  const auto selected = settings.selection_bounds.normalized().intersected(full);
  return selected.isEmpty() ? full : selected;
}

void draw_crop_marks(QPainter& painter, const QRectF& target) {
  constexpr double kLength = 18.0;
  constexpr double kGap = 5.0;
  const auto left = target.left();
  const auto right = target.right();
  const auto top = target.top();
  const auto bottom = target.bottom();

  painter.save();
  QPen pen(QColor(20, 20, 20), 0.0);
  pen.setCosmetic(true);
  painter.setPen(pen);
  painter.drawLine(QPointF(left - kGap - kLength, top), QPointF(left - kGap, top));
  painter.drawLine(QPointF(left, top - kGap - kLength), QPointF(left, top - kGap));
  painter.drawLine(QPointF(right + kGap, top), QPointF(right + kGap + kLength, top));
  painter.drawLine(QPointF(right, top - kGap - kLength), QPointF(right, top - kGap));
  painter.drawLine(QPointF(left - kGap - kLength, bottom), QPointF(left - kGap, bottom));
  painter.drawLine(QPointF(left, bottom + kGap), QPointF(left, bottom + kGap + kLength));
  painter.drawLine(QPointF(right + kGap, bottom), QPointF(right + kGap + kLength, bottom));
  painter.drawLine(QPointF(right, bottom + kGap), QPointF(right, bottom + kGap + kLength));
  painter.restore();
}

}  // namespace

QPageLayout default_print_page_layout() {
  return QPageLayout(QPageSize(QPageSize::Letter), QPageLayout::Portrait, QMarginsF(0.5, 0.5, 0.5, 0.5),
                     QPageLayout::Inch);
}

std::vector<QPageSize::PageSizeId> print_page_size_choices() {
  return {QPageSize::Letter, QPageSize::Legal,  QPageSize::Tabloid, QPageSize::Executive, QPageSize::A3,
          QPageSize::A4,     QPageSize::A5,     QPageSize::A6,      QPageSize::B4,        QPageSize::B5,
          QPageSize::Custom};
}

QPageSize custom_page_size_points(QSizeF points) {
  if (!(points.width() > 0.0) || !(points.height() > 0.0)) {
    return {};
  }
  return QPageSize(points, QPageSize::Point, QString(), QPageSize::ExactMatch);
}

QPageLayout page_layout_with_size(const QPageLayout& layout, const QPageSize& size,
                                  QPageLayout::Orientation orientation) {
  auto result = valid_page_layout(layout);
  if (!size.isValid()) {
    return result;
  }
  // The zero minimum margins drop whatever printer minimums the layout was born
  // with: an in-app sheet has no printer, so the whole sheet is addressable.
  result.setPageSize(size, QMarginsF(0.0, 0.0, 0.0, 0.0));
  result.setOrientation(orientation);
  result.setMargins(layout.margins(result.units()));
  if (!result.isValid() || result.paintRect(QPageLayout::Point).isEmpty()) {
    result.setMargins(QMarginsF(0.0, 0.0, 0.0, 0.0));
  }
  return result;
}

QPageLayout artboard_page_layout(const Document& page) {
  const auto size = custom_page_size_points(
      QSizeF(static_cast<double>(std::max<std::int32_t>(1, page.width())) / document_horizontal_ppi(page) *
                 kPointsPerInch,
             static_cast<double>(std::max<std::int32_t>(1, page.height())) / document_vertical_ppi(page) *
                 kPointsPerInch));
  QPageLayout layout(size, QPageLayout::Portrait, QMarginsF(0.0, 0.0, 0.0, 0.0), QPageLayout::Point,
                     QMarginsF(0.0, 0.0, 0.0, 0.0));
  return valid_page_layout(layout);
}

std::vector<Document> print_artboard_pages(const Document& document) {
  return documents_for_artboards(document);
}

PrintSettings artboard_page_print_settings(const PrintSettings& settings) {
  auto page = settings;
  page.area_mode = PrintAreaMode::Document;
  page.selection_bounds = QRect();
  if (settings.match_artboard_paper) {
    page.scale_mode = PrintScaleMode::ActualSize;
    page.scale_percent = 100.0;
    page.center = true;
    page.crop_marks = false;
  }
  return page;
}

namespace {

constexpr const char* kPrintPageSizeIdKey = "print/pageSizeId";
constexpr const char* kPrintOrientationKey = "print/orientation";
constexpr const char* kPrintCustomWidthKey = "print/customWidthPoints";
constexpr const char* kPrintCustomHeightKey = "print/customHeightPoints";
constexpr const char* kPrintMarginLeftKey = "print/marginLeftPoints";
constexpr const char* kPrintMarginTopKey = "print/marginTopPoints";
constexpr const char* kPrintMarginRightKey = "print/marginRightPoints";
constexpr const char* kPrintMarginBottomKey = "print/marginBottomPoints";

}  // namespace

QPageLayout load_stored_print_page_layout() {
  auto settings = app_settings();
  if (!settings.contains(QLatin1String(kPrintPageSizeIdKey))) {
    return default_print_page_layout();
  }
  const int id = settings.value(QLatin1String(kPrintPageSizeIdKey)).toInt();
  QPageSize size;
  if (id == static_cast<int>(QPageSize::Custom)) {
    size = custom_page_size_points(QSizeF(settings.value(QLatin1String(kPrintCustomWidthKey)).toDouble(),
                                          settings.value(QLatin1String(kPrintCustomHeightKey)).toDouble()));
  } else if (id >= 0 && id < static_cast<int>(QPageSize::LastPageSize)) {
    size = QPageSize(static_cast<QPageSize::PageSizeId>(id));
  }
  if (!size.isValid()) {
    return default_print_page_layout();
  }
  const auto orientation = settings.value(QLatin1String(kPrintOrientationKey), 0).toInt() == 1
                               ? QPageLayout::Landscape
                               : QPageLayout::Portrait;
  const QMarginsF margins(settings.value(QLatin1String(kPrintMarginLeftKey), 36.0).toDouble(),
                          settings.value(QLatin1String(kPrintMarginTopKey), 36.0).toDouble(),
                          settings.value(QLatin1String(kPrintMarginRightKey), 36.0).toDouble(),
                          settings.value(QLatin1String(kPrintMarginBottomKey), 36.0).toDouble());
  QPageLayout layout(size, orientation, margins, QPageLayout::Point);
  if (!layout.isValid() || layout.paintRect(QPageLayout::Point).isEmpty()) {
    return default_print_page_layout();
  }
  return layout;
}

void store_print_page_layout(const QPageLayout& layout) {
  const auto valid = valid_page_layout(layout);
  auto settings = app_settings();
  const auto size = valid.pageSize();
  settings.setValue(QLatin1String(kPrintPageSizeIdKey), static_cast<int>(size.id()));
  const auto sheet = size.size(QPageSize::Point);
  settings.setValue(QLatin1String(kPrintCustomWidthKey), sheet.width());
  settings.setValue(QLatin1String(kPrintCustomHeightKey), sheet.height());
  settings.setValue(QLatin1String(kPrintOrientationKey), valid.orientation() == QPageLayout::Landscape ? 1 : 0);
  const auto margins = valid.margins(QPageLayout::Point);
  settings.setValue(QLatin1String(kPrintMarginLeftKey), margins.left());
  settings.setValue(QLatin1String(kPrintMarginTopKey), margins.top());
  settings.setValue(QLatin1String(kPrintMarginRightKey), margins.right());
  settings.setValue(QLatin1String(kPrintMarginBottomKey), margins.bottom());
}

PrintSettings default_print_settings(const Document& document, std::optional<QRect> selection_bounds) {
  PrintSettings settings;
  if (selection_bounds.has_value()) {
    settings.selection_bounds = selection_bounds->normalized().intersected(document_rect(document));
  }
  return settings;
}

PrintPlacement calculate_print_placement(const Document& document, const PrintSettings& settings,
                                         const QPageLayout& page_layout) {
  const auto layout = valid_page_layout(page_layout);
  const auto source = source_rect_for_settings(document, settings);
  // Per-axis PPI: anisotropic documents (scanner files, some BMPs) print at their true
  // physical size instead of stretching the vertical axis to the horizontal density.
  const QSizeF actual_points(
      static_cast<double>(source.width()) / document_horizontal_ppi(document) * kPointsPerInch,
      static_cast<double>(source.height()) / document_vertical_ppi(document) * kPointsPerInch);

  QSizeF target_size = actual_points;
  double effective_scale = 100.0;
  const auto printable = layout.paintRect(QPageLayout::Point);
  if (settings.scale_mode == PrintScaleMode::FitToPage && actual_points.width() > 0.0 &&
      actual_points.height() > 0.0 && printable.width() > 0.0 && printable.height() > 0.0) {
    const auto fit = std::min(printable.width() / actual_points.width(), printable.height() / actual_points.height());
    target_size = QSizeF(actual_points.width() * fit, actual_points.height() * fit);
    effective_scale = fit * 100.0;
  } else if (settings.scale_mode == PrintScaleMode::CustomScale) {
    effective_scale = sanitized_scale_percent(settings.scale_percent);
    target_size = QSizeF(actual_points.width() * (effective_scale / 100.0),
                         actual_points.height() * (effective_scale / 100.0));
  }

  QPointF origin;
  if (settings.center) {
    origin = QPointF(printable.left() + (printable.width() - target_size.width()) / 2.0,
                     printable.top() + (printable.height() - target_size.height()) / 2.0);
  } else {
    origin = QPointF(printable.left() + settings.offset_x_inches * kPointsPerInch,
                     printable.top() + settings.offset_y_inches * kPointsPerInch);
  }

  return PrintPlacement{source, QRectF(origin, target_size), effective_scale,
                        QSizeF(target_size.width() / kPointsPerInch, target_size.height() / kPointsPerInch)};
}

void render_print_page(QPainter& painter, const Document& document, const PrintSettings& settings,
                       const QPageLayout& page_layout, bool draw_printable_guide) {
  const auto layout = valid_page_layout(page_layout);
  const auto page = layout.fullRect(QPageLayout::Point);
  const auto printable = layout.paintRect(QPageLayout::Point);
  painter.save();
  painter.fillRect(page, Qt::white);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const auto placement = calculate_print_placement(document, settings, layout);
  const auto image = qimage_from_document_rect(document, placement.source_rect, false);
  if (!image.isNull()) {
    painter.drawImage(placement.target_rect_points, image, QRectF(0, 0, image.width(), image.height()));
  }
  if (settings.crop_marks) {
    draw_crop_marks(painter, placement.target_rect_points);
  }

  // The dashed printable-area outline is a preview aid only; a print job or a saved PDF
  // must never carry it.
  if (draw_printable_guide) {
    QPen printable_pen(QColor(205, 205, 205), 0.0, Qt::DashLine);
    printable_pen.setCosmetic(true);
    painter.setPen(printable_pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(printable);
  }
  painter.restore();
}

QString default_print_pdf_filename(const QString& document_title) {
  const auto title = document_title.isEmpty() ? QObject::tr("Untitled") : document_title;
  return QFileInfo(title).completeBaseName() + QStringLiteral(".pdf");
}

}  // namespace patchy::ui
