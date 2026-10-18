#pragma once

#include "ui/document_order_list.hpp"
#include "ui/pdf_export.hpp"

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

class QWidget;

namespace patchy::ui {

// File > Export Multi-Page PDF: which pages go into the file and how. The writing
// itself is write_multipage_pdf_file (pdf_export.hpp); MainWindow resolves the choice
// into documents because only it owns the sessions.

// One open document as the dialog lists it (the shared document-order row).
using MultiPagePdfDocumentEntry = DocumentOrderEntry;

enum class MultiPagePdfSource {
  // Every checked open document, one page each, in the list's order.
  OpenDocuments,
  // Every top-level layer group of the active document (hidden ones too unless the
  // choice says otherwise), one page each, top of the layer stack first ("print a folder
  // as a page"; a PDF imported as page folders goes back page by page).
  TopLevelGroups,
  // Every visible artboard of the active document, one page each, sized to it
  // (documents_for_artboards, docs/artboards.md).
  Artboards,
};

struct MultiPagePdfExportChoice {
  MultiPagePdfSource source{MultiPagePdfSource::OpenDocuments};
  // OpenDocuments: the checked sessions in page order.
  std::vector<std::int64_t> session_ids;
  // TopLevelGroups: ungrouped root layers (a shared background, say) draw on every page.
  bool include_ungrouped_layers{true};
  // TopLevelGroups: hidden groups are pages too (a PDF imported as page folders shows
  // only page 1). Persists as exportOptions/multiPagePdfHiddenGroups.
  bool include_hidden_groups{true};
  PdfExportOptions options;
};

// `documents` lists every open session; `active_session_id` names the one whose groups
// the TopLevelGroups source would page (that source is offered only when
// `top_level_group_count` is at least one). `original_image_data_available` says some
// open document still carries the image data it was imported from a PDF with, which is
// the only time the "keep original image data" checkbox is shown. nullopt when cancelled.
[[nodiscard]] std::optional<MultiPagePdfExportChoice> run_multipage_pdf_export_dialog(
    QWidget* parent, const std::vector<MultiPagePdfDocumentEntry>& documents, std::int64_t active_session_id,
    int top_level_group_count, bool original_image_data_available = false, int artboard_count = 0,
    int hidden_group_count = 0);

}  // namespace patchy::ui
