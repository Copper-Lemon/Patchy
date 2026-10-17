#include "ui/export_documents_folder_dialog.hpp"

#include "ui/app_settings.hpp"
#include "ui/dialog_utils.hpp"
#include "ui/theme_qss.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

namespace patchy::ui {
namespace {

constexpr auto kFolderKey = "exportDocumentsFolder/folder";
constexpr auto kPrefixKey = "exportDocumentsFolder/prefix";
constexpr auto kStartKey = "exportDocumentsFolder/start";
constexpr auto kPaddingKey = "exportDocumentsFolder/padding";
constexpr auto kFormatKey = "exportDocumentsFolder/format";
constexpr auto kExistingFilesKey = "exportDocumentsFolder/existingFiles";

}  // namespace

std::optional<ExportDocumentsFolderChoice> run_export_documents_folder_dialog(
    QWidget* parent, const std::vector<DocumentOrderEntry>& documents, std::int64_t active_session_id,
    const std::vector<ExportDocumentsFormatChoice>& formats, const QString& initial_folder,
    ExportDocumentsFolderSource source, const QString& artboard_prefix) {
  auto settings = app_settings();
  const bool artboards = source == ExportDocumentsFolderSource::Artboards;
  const QString title = artboards ? QObject::tr("Export Artboards to Files") : QObject::tr("Export Documents to Folder");

  QDialog dialog(parent);
  dialog.setObjectName(artboards ? QStringLiteral("exportArtboardsDialog") : QStringLiteral("exportDocumentsFolderDialog"));
  dialog.setWindowTitle(title);
  dialog.resize(540, 520);
  auto* layout = new QVBoxLayout(&dialog);

  auto* documents_group = new QGroupBox(artboards ? QObject::tr("Artboards") : QObject::tr("Documents"), &dialog);
  auto* documents_layout = new QVBoxLayout(documents_group);
  const auto order = build_document_order_controls(documents_group, QStringLiteral("exportDocuments"), documents,
                                                   active_session_id);
  auto* list = order.list;
  documents_layout->addWidget(order.row);
  layout->addWidget(documents_group, 1);

  auto* output_group = new QGroupBox(QObject::tr("Output"), &dialog);
  auto* output_form = new QFormLayout(output_group);
  auto* folder_row = new QWidget(output_group);
  auto* folder_row_layout = new QHBoxLayout(folder_row);
  folder_row_layout->setContentsMargins(0, 0, 0, 0);
  folder_row_layout->setSpacing(4);
  auto* folder_edit = new QLineEdit(folder_row);
  folder_edit->setObjectName(QStringLiteral("exportDocumentsFolderEdit"));
  folder_edit->setText(QDir::toNativeSeparators(settings.value(QLatin1String(kFolderKey), initial_folder).toString()));
  auto* browse_button = new QPushButton(QStringLiteral("..."), folder_row);
  browse_button->setObjectName(QStringLiteral("exportDocumentsFolderBrowseButton"));
  browse_button->setToolTip(QObject::tr("Choose Folder..."));
  configure_compact_symbol_button(browse_button);
  folder_row_layout->addWidget(folder_edit, 1);
  folder_row_layout->addWidget(browse_button, 0);
  output_form->addRow(QObject::tr("Folder"), folder_row);

  auto* prefix_edit = new QLineEdit(output_group);
  prefix_edit->setObjectName(QStringLiteral("exportDocumentsPrefixEdit"));
  prefix_edit->setText(artboards ? artboard_prefix
                                 : settings.value(QLatin1String(kPrefixKey), QStringLiteral("page_")).toString());
  output_form->addRow(QObject::tr("Prefix"), prefix_edit);

  auto* numbering_row = new QWidget(output_group);
  auto* numbering_layout = new QHBoxLayout(numbering_row);
  numbering_layout->setContentsMargins(0, 0, 0, 0);
  auto* start_spin = new QSpinBox(numbering_row);
  start_spin->setObjectName(QStringLiteral("exportDocumentsStartSpin"));
  start_spin->setRange(0, 999999);
  start_spin->setValue(std::clamp(settings.value(QLatin1String(kStartKey), 1).toInt(), 0, 999999));
  auto* padding_label = new QLabel(QObject::tr("Digits"), numbering_row);
  auto* padding_spin = new QSpinBox(numbering_row);
  padding_spin->setObjectName(QStringLiteral("exportDocumentsPaddingSpin"));
  padding_spin->setRange(1, 6);
  padding_spin->setValue(std::clamp(settings.value(QLatin1String(kPaddingKey), 3).toInt(), 1, 6));
  padding_label->setBuddy(padding_spin);
  configure_dialog_spinbox(start_spin);
  configure_dialog_spinbox(padding_spin);
  numbering_layout->addWidget(start_spin, 1);
  numbering_layout->addSpacing(8);
  numbering_layout->addWidget(padding_label);
  numbering_layout->addWidget(padding_spin, 1);
  output_form->addRow(QObject::tr("Start at"), numbering_row);
  // Artboard files are named for their artboards, not numbered.
  output_form->setRowVisible(numbering_row, !artboards);

  auto* format_combo = new QComboBox(output_group);
  format_combo->setObjectName(QStringLiteral("exportDocumentsFormatCombo"));
  for (const auto& choice : formats) {
    format_combo->addItem(choice.display_name, choice.extension);
  }
  {
    int format_index = format_combo->findData(settings.value(QLatin1String(kFormatKey), QStringLiteral("png")));
    if (format_index < 0) {
      format_index = format_combo->findData(QStringLiteral("png"));
    }
    format_combo->setCurrentIndex(std::max(0, format_index));
  }
  output_form->addRow(QObject::tr("Format"), format_combo);

  auto* existing_combo = new QComboBox(output_group);
  existing_combo->setObjectName(QStringLiteral("exportDocumentsExistingCombo"));
  existing_combo->addItem(QObject::tr("Add"), static_cast<int>(ExportDocumentsExistingFiles::AddNumbering));
  existing_combo->addItem(QObject::tr("Overwrite"), static_cast<int>(ExportDocumentsExistingFiles::Overwrite));
  existing_combo->setToolTip(
      artboards ? QObject::tr("Add puts a number after a name that is already taken; Overwrite asks before "
                              "replacing anything.")
                : QObject::tr("Add continues numbering after the files already in the folder; Overwrite "
                              "starts at the chosen number and asks before replacing anything."));
  existing_combo->setCurrentIndex(
      settings.value(QLatin1String(kExistingFilesKey), static_cast<int>(ExportDocumentsExistingFiles::AddNumbering))
                  .toInt() == static_cast<int>(ExportDocumentsExistingFiles::Overwrite)
          ? 1
          : 0);
  output_form->addRow(QObject::tr("If files exist"), existing_combo);
  layout->addWidget(output_group);

  auto* preview = new QLabel(&dialog);
  preview->setObjectName(QStringLiteral("exportDocumentsPreviewLabel"));
  preview->setWordWrap(true);
  layout->addWidget(preview);
  auto* summary = new QLabel(&dialog);
  summary->setObjectName(QStringLiteral("exportDocumentsSummaryLabel"));
  summary->setWordWrap(true);
  layout->addWidget(summary);

  auto* buttons = new QDialogButtonBox(&dialog);
  auto* export_button = buttons->addButton(QObject::tr("Export"), QDialogButtonBox::AcceptRole);
  export_button->setObjectName(QStringLiteral("exportDocumentsExportButton"));
  export_button->setDefault(true);
  buttons->addButton(QDialogButtonBox::Cancel);
  layout->addWidget(buttons);

  const auto current_naming = [prefix_edit, start_spin, padding_spin, artboards] {
    ImageSequenceNaming naming;
    naming.use_layer_names = artboards;
    // An artboard prefix is joined to each name before sanitizing (image_sequence_file_names).
    naming.prefix = artboards ? prefix_edit->text() : sanitized_file_name(prefix_edit->text());
    naming.start = start_spin->value();
    naming.padding = padding_spin->value();
    return naming;
  };
  std::map<std::int64_t, QString> titles;
  for (const auto& entry : documents) {
    titles[entry.session_id] = entry.title;
  }
  const auto sync = [&] {
    const auto ids = selected_session_ids(*list);
    const int count = static_cast<int>(ids.size());
    const bool folder_ok = !folder_edit->text().trimmed().isEmpty();
    export_button->setEnabled(count > 0 && folder_ok);
    if (count == 0) {
      preview->clear();
      summary->setText(artboards ? QObject::tr("Select at least one artboard.")
                                 : QObject::tr("Select at least one document."));
      return;
    }
    const auto naming = current_naming();
    std::vector<QString> placeholders(static_cast<std::size_t>(count));
    if (artboards) {
      for (std::size_t index = 0; index < ids.size(); ++index) {
        placeholders[index] = naming.prefix + titles[ids[index]];
      }
    }
    const auto names = image_sequence_file_names(placeholders, naming, format_combo->currentData().toString());
    QStringList shown;
    for (qsizetype index = 0; index < std::min<qsizetype>(3, names.size()); ++index) {
      shown.push_back(names[index]);
    }
    if (names.size() > 4) {
      shown.push_back(QStringLiteral("..."));
    }
    if (names.size() > 3) {
      shown.push_back(names.back());
    }
    preview->setText(shown.join(QStringLiteral(", ")));
    summary->setText(folder_ok ? QObject::tr("%n file(s) will be written.", nullptr, count)
                               : QObject::tr("Choose a folder."));
  };
  sync();
  QObject::connect(list, &QListWidget::itemSelectionChanged, &dialog, sync);
  for (auto* button : {order.select_all, order.move_up, order.move_down, order.auto_sort, order.reverse}) {
    QObject::connect(button, &QPushButton::clicked, &dialog, sync);
  }
  QObject::connect(folder_edit, &QLineEdit::textChanged, &dialog, sync);
  QObject::connect(prefix_edit, &QLineEdit::textChanged, &dialog, sync);
  QObject::connect(start_spin, &QSpinBox::valueChanged, &dialog, sync);
  QObject::connect(padding_spin, &QSpinBox::valueChanged, &dialog, sync);
  QObject::connect(format_combo, &QComboBox::currentIndexChanged, &dialog, sync);
  QObject::connect(browse_button, &QPushButton::clicked, &dialog, [&dialog, folder_edit] {
    const auto chosen = QFileDialog::getExistingDirectory(&dialog, QObject::tr("Choose Folder"), folder_edit->text());
    if (!chosen.isEmpty()) {
      folder_edit->setText(QDir::toNativeSeparators(chosen));
    }
  });
  // Validating accept: the folder must exist (or be creatable) before the dialog
  // closes; on failure it stays open for a correction.
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, folder_edit, &title] {
    const QString folder = folder_edit->text().trimmed();
    if (folder.isEmpty() || !QDir().mkpath(folder)) {
      (void)show_warning_message(&dialog, title,
                                 QObject::tr("The folder \"%1\" could not be created.").arg(folder),
                                 QMessageBox::Ok, QMessageBox::Ok,
                                 QStringLiteral("exportDocumentsFolderCreateFailedMessageBox"));
      return;
    }
    dialog.accept();
  });
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  append_themed_style(dialog, dialog_spinbox_button_style());

  remember_dialog_position(dialog);
  if (exec_dialog(dialog) != QDialog::Accepted) {
    return std::nullopt;
  }

  ExportDocumentsFolderChoice choice;
  choice.session_ids = selected_session_ids(*list);
  choice.folder = QDir(folder_edit->text().trimmed()).absolutePath();
  choice.extension = format_combo->currentData().toString();
  choice.naming = current_naming();
  choice.existing_files = static_cast<ExportDocumentsExistingFiles>(existing_combo->currentData().toInt());
  settings.setValue(QLatin1String(kFolderKey), choice.folder);
  if (!artboards) {
    settings.setValue(QLatin1String(kPrefixKey), choice.naming.prefix);
    settings.setValue(QLatin1String(kStartKey), choice.naming.start);
    settings.setValue(QLatin1String(kPaddingKey), choice.naming.padding);
  }
  settings.setValue(QLatin1String(kFormatKey), choice.extension);
  settings.setValue(QLatin1String(kExistingFilesKey), static_cast<int>(choice.existing_files));
  return choice;
}

}  // namespace patchy::ui
