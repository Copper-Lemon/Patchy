#pragma once

#include "core/document.hpp"

#include <QPageLayout>
#include <QPageSize>
#include <QRect>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <optional>
#include <vector>

class QPainter;
class QWidget;

namespace patchy::ui {

enum class PrintAreaMode {
  Document,
  Selection,
  // One page per visible artboard (documents_for_artboards), docs/artboards.md.
  Artboards
};

enum class PrintScaleMode {
  ActualSize,
  FitToPage,
  CustomScale
};

// Print size derives from the document's own resolution (Photoshop semantics): the
// on-paper size at 100% is pixels / document PPI, per axis. The dialog surfaces the
// effective print resolution as a read-only value (document PPI / scale); editing
// resolution belongs to Image Size, not the print flow.
struct PrintSettings {
  PrintAreaMode area_mode{PrintAreaMode::Document};
  QRect selection_bounds;
  PrintScaleMode scale_mode{PrintScaleMode::ActualSize};
  double scale_percent{100.0};
  bool center{true};
  double offset_x_inches{0.0};
  double offset_y_inches{0.0};
  bool crop_marks{false};
  // Artboards only: each sheet is its artboard's print size with no margins
  // (artboard_page_layout), printed at actual size. A PDF changes sheet per page; a
  // printer driver cannot, so real print jobs keep the chosen paper.
  bool match_artboard_paper{false};
};

struct PrintPlacement {
  QRect source_rect;
  QRectF target_rect_points;
  double scale_percent{100.0};
  QSizeF print_size_inches;
};

[[nodiscard]] QPageLayout default_print_page_layout();
// The in-app paper controls (print_layout.cpp, portable): sheet sizes the Print
// dialog offers without a printer driver, Custom last. Page Setup through the OS
// dialog still works; these exist because a PDF written from the dialog needs no
// driver and a driver-backed setup cannot express an arbitrary sheet.
[[nodiscard]] std::vector<QPageSize::PageSizeId> print_page_size_choices();
// An exact sheet in points (QPageSize::ExactMatch, so a nearly-A4 sheet never snaps
// to A4); invalid when either edge is not positive.
[[nodiscard]] QPageSize custom_page_size_points(QSizeF points);
// `layout` with another sheet and orientation. Margins and units are kept; margins
// the new sheet cannot hold collapse to zero so the result is always valid.
[[nodiscard]] QPageLayout page_layout_with_size(const QPageLayout& layout, const QPageSize& size,
                                                QPageLayout::Orientation orientation);
// A margin-free sheet exactly the page's print size (pixels / document PPI), portrait.
[[nodiscard]] QPageLayout artboard_page_layout(const Document& page);
// The pages PrintAreaMode::Artboards prints, top artboard first; empty for a document
// without visible artboards.
[[nodiscard]] std::vector<Document> print_artboard_pages(const Document& document);
// `settings` for one artboard page: the whole page, and with match_artboard_paper,
// actual size, centered, no crop marks.
[[nodiscard]] PrintSettings artboard_page_print_settings(const PrintSettings& settings);
// The user's last accepted page layout (settings group "print"); the Letter default
// when nothing is stored or the stored sheet is unusable.
[[nodiscard]] QPageLayout load_stored_print_page_layout();
void store_print_page_layout(const QPageLayout& layout);
[[nodiscard]] PrintSettings default_print_settings(const Document& document, std::optional<QRect> selection_bounds);
[[nodiscard]] PrintPlacement calculate_print_placement(const Document& document, const PrintSettings& settings,
                                                       const QPageLayout& page_layout);
void render_print_page(QPainter& painter, const Document& document, const PrintSettings& settings,
                       const QPageLayout& page_layout,
                       bool draw_printable_guide = false);
[[nodiscard]] bool write_print_pdf(const QString& path, const Document& document, const PrintSettings& settings,
                                   const QPageLayout& page_layout, const QString& document_name = {});
// "photo.psd" -> "photo.pdf"; empty title -> "Untitled.pdf".
[[nodiscard]] QString default_print_pdf_filename(const QString& document_title);
void run_page_setup_dialog(QWidget* parent, QPageLayout* page_layout);
[[nodiscard]] bool run_print_dialog(QWidget* parent, const Document& document, const QString& document_title,
                                    std::optional<QRect> selection_bounds, QPageLayout* page_layout);
// The print half of File > Import > Photocopy: the freshly scanned document prints at
// actual size, centered, never scaled (matching the original's physical size is the
// feature's contract), with a preview shading whatever the printable area cuts off.
// Returns true when a print job was sent.
[[nodiscard]] bool run_photocopy_dialog(QWidget* parent, const Document& document);

}  // namespace patchy::ui
