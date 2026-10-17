#include "ui/multipage_pdf_export_dialog.hpp"

#include "ui/app_settings.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/image_save_options_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <algorithm>

namespace patchy::ui {
namespace {

constexpr auto kSourceKey = "exportOptions/multiPagePdfSource";
constexpr auto kUngroupedKey = "exportOptions/multiPagePdfUngroupedLayers";
constexpr auto kEditableKey = "exportOptions/multiPagePdfEditableLayers";
constexpr auto kMissingFontsKey = "saveOptions/pdfMissingFontsAsImages";
constexpr auto kKeepOriginalKey = "saveOptions/pdfKeepOriginalImages";

}  // namespace

std::optional<MultiPagePdfExportChoice> run_multipage_pdf_export_dialog(
    QWidget* parent, const std::vector<MultiPagePdfDocumentEntry>& documents, std::int64_t active_session_id,
    int top_level_group_count, bool original_image_data_available, int artboard_count) {
  auto settings = app_settings();
  const bool groups_available = top_level_group_count > 0;
  const bool artboards_available = artboard_count > 0;
  const auto stored_source = settings.value(QLatin1String(kSourceKey), QStringLiteral("documents")).toString();
  const bool stored_groups = stored_source == QStringLiteral("groups");

  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("multiPagePdfExportDialog"));
  dialog.setWindowTitle(QObject::tr("Export Multi-Page PDF"));
  dialog.resize(520, 460);
  auto* layout = new QVBoxLayout(&dialog);

  auto* source_group = new QGroupBox(QObject::tr("Pages"), &dialog);
  auto* source_layout = new QVBoxLayout(source_group);

  auto* documents_radio = new QRadioButton(QObject::tr("One page per open document"), source_group);
  documents_radio->setObjectName(QStringLiteral("multiPagePdfDocumentsRadio"));
  source_layout->addWidget(documents_radio);

  // The shared ordered checklist: Move Up/Down, Auto Sort (numbering-aware), Reverse.
  const auto order = build_document_order_controls(source_group, QStringLiteral("multiPagePdf"), documents,
                                                   active_session_id);
  auto* list = order.list;
  source_layout->addWidget(order.row);

  auto* groups_radio = new QRadioButton(
      groups_available ? QObject::tr("One page per top-level layer group of the current document (%n group(s))",
                                     nullptr, top_level_group_count)
                       : QObject::tr("One page per top-level layer group (the current document has none)"),
      source_group);
  groups_radio->setObjectName(QStringLiteral("multiPagePdfGroupsRadio"));
  groups_radio->setEnabled(groups_available);
  source_layout->addWidget(groups_radio);
  auto* artboards_radio = new QRadioButton(
      artboards_available ? QObject::tr("One page per artboard of the current document (%n artboard(s))", nullptr,
                                        artboard_count)
                          : QObject::tr("One page per artboard (the current document has none)"),
      source_group);
  artboards_radio->setObjectName(QStringLiteral("multiPagePdfArtboardsRadio"));
  artboards_radio->setEnabled(artboards_available);
  source_layout->addWidget(artboards_radio);
  auto* ungrouped = new QCheckBox(QObject::tr("Draw ungrouped layers on every page"), source_group);
  ungrouped->setObjectName(QStringLiteral("multiPagePdfUngroupedCheck"));
  ungrouped->setChecked(settings.value(QLatin1String(kUngroupedKey), true).toBool());
  source_layout->addWidget(ungrouped);
  layout->addWidget(source_group);

  auto* options_group = new QGroupBox(QObject::tr("PDF Options"), &dialog);
  auto* options_layout = new QVBoxLayout(options_group);
  auto* editable = new QCheckBox(QObject::tr("Keep layers as editable objects"), options_group);
  editable->setObjectName(QStringLiteral("multiPagePdfEditableCheck"));
  // On by default: a multi-page PDF is a print or handoff document, and text and
  // shapes that stay editable are worth more there than pixel-exact blends.
  editable->setChecked(settings.value(QLatin1String(kEditableKey), true).toBool());
  options_layout->addWidget(editable);
  auto* editable_note = new QLabel(
      QObject::tr("Shapes and text stay editable; blend modes, adjustment layers, and layer styles flatten to "
                  "images on each page."),
      options_group);
  editable_note->setObjectName(QStringLiteral("multiPagePdfEditableNote"));
  editable_note->setWordWrap(true);
  options_layout->addWidget(editable_note);
  // Shared with the single-page PDF Options dialog: one image-quality preference.
  auto* quality_row = new QHBoxLayout();
  auto* quality_label = new QLabel(QObject::tr("Image quality:"), options_group);
  auto* quality = new QComboBox(options_group);
  quality->setObjectName(QStringLiteral("multiPagePdfImageQualityCombo"));
  populate_pdf_image_quality_combo(*quality, stored_pdf_image_quality_id());
  quality_label->setBuddy(quality);
  quality_row->addWidget(quality_label);
  quality_row->addWidget(quality, 1);
  options_layout->addLayout(quality_row);
  auto* keep_original = new QCheckBox(pdf_keep_original_images_label(), options_group);
  keep_original->setObjectName(QStringLiteral("multiPagePdfKeepOriginalCheck"));
  keep_original->setChecked(settings.value(QLatin1String(kKeepOriginalKey), true).toBool());
  keep_original->setToolTip(pdf_keep_original_images_tooltip());
  keep_original->setVisible(original_image_data_available);
  options_layout->addWidget(keep_original);
  layout->addWidget(options_group);

  auto* summary = new QLabel(&dialog);
  summary->setObjectName(QStringLiteral("multiPagePdfSummaryLabel"));
  summary->setWordWrap(true);
  layout->addWidget(summary);
  layout->addStretch(1);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* export_button = buttons->addButton(QObject::tr("Export..."), QDialogButtonBox::AcceptRole);
  export_button->setObjectName(QStringLiteral("multiPagePdfExportButton"));
  export_button->setDefault(true);
  buttons->addButton(QDialogButtonBox::Cancel);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);

  const auto sync = [&] {
    const bool by_groups = groups_radio->isChecked();
    const bool by_artboards = artboards_radio->isChecked();
    sync_document_order_controls(order, !by_groups && !by_artboards);
    ungrouped->setEnabled(by_groups);
    editable_note->setEnabled(editable->isChecked());
    const int page_count = by_groups      ? top_level_group_count
                           : by_artboards ? artboard_count
                                          : static_cast<int>(selected_session_ids(*list).size());
    export_button->setEnabled(page_count > 0);
    summary->setText(page_count > 0 ? QObject::tr("%n page(s) will be written.", nullptr, page_count)
                                    : QObject::tr("Select at least one document."));
  };
  // An artboard document opens on its artboards: that is what such a document's pages are.
  (artboards_available && (stored_source == QStringLiteral("artboards") || !stored_groups) ? artboards_radio
   : groups_available && stored_groups                                                   ? groups_radio
                                                                                          : documents_radio)
      ->setChecked(true);
  sync();
  QObject::connect(documents_radio, &QRadioButton::toggled, &dialog, sync);
  QObject::connect(groups_radio, &QRadioButton::toggled, &dialog, sync);
  QObject::connect(artboards_radio, &QRadioButton::toggled, &dialog, sync);
  QObject::connect(list, &QListWidget::itemSelectionChanged, &dialog, sync);
  QObject::connect(list, &QListWidget::currentRowChanged, &dialog, sync);
  QObject::connect(order.select_all, &QPushButton::clicked, &dialog, sync);
  QObject::connect(editable, &QCheckBox::toggled, &dialog, sync);

  remember_dialog_position(dialog);
  if (exec_dialog(dialog) != QDialog::Accepted) {
    return std::nullopt;
  }

  MultiPagePdfExportChoice choice;
  choice.source = groups_radio->isChecked()      ? MultiPagePdfSource::TopLevelGroups
                  : artboards_radio->isChecked() ? MultiPagePdfSource::Artboards
                                                 : MultiPagePdfSource::OpenDocuments;
  choice.session_ids = selected_session_ids(*list);
  choice.include_ungrouped_layers = ungrouped->isChecked();
  apply_pdf_image_quality(quality->currentData().toString(), choice.options);
  choice.options.editable_layers = editable->isChecked();
  choice.options.missing_fonts_as_images = settings.value(QLatin1String(kMissingFontsKey), false).toBool();
  settings.setValue(QLatin1String(kSourceKey), choice.source == MultiPagePdfSource::TopLevelGroups ? QStringLiteral("groups")
                                                : choice.source == MultiPagePdfSource::Artboards
                                                    ? QStringLiteral("artboards")
                                                    : QStringLiteral("documents"));
  settings.setValue(QLatin1String(kUngroupedKey), choice.include_ungrouped_layers);
  settings.setValue(QLatin1String(kEditableKey), choice.options.editable_layers);
  store_pdf_image_quality_id(quality->currentData().toString());
  choice.options.keep_original_image_data = settings.value(QLatin1String(kKeepOriginalKey), true).toBool();
  if (original_image_data_available) {
    choice.options.keep_original_image_data = keep_original->isChecked();
    settings.setValue(QLatin1String(kKeepOriginalKey), choice.options.keep_original_image_data);
  }
  return choice;
}

}  // namespace patchy::ui
