#include "ui/missing_fonts_dialog.hpp"

#include "ui/dialog_utils.hpp"
#include "ui/font_picker.hpp"
#include "ui/text_font_service.hpp"
#include "ui/theme_qss.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace patchy::ui {

std::optional<MissingFontsDialogResult> run_missing_fonts_dialog(QWidget* parent,
                                                                 const std::vector<MissingFontEntry>& fonts,
                                                                 bool show_on_open, bool* show_on_open_out) {
  QDialog dialog(parent);
  dialog.setObjectName(QStringLiteral("missingFontsDialog"));
  dialog.setWindowTitle(QObject::tr("Missing Fonts"));
  auto* layout = new QVBoxLayout(&dialog);

  auto* intro = new QLabel(QObject::tr("Some fonts used in this document are missing. Choose an installed font to "
                                       "replace each one in every text layer that uses it, or leave it unresolved."),
                           &dialog);
  intro->setObjectName(QStringLiteral("missingFontsIntroLabel"));
  intro->setWordWrap(true);
  layout->addWidget(intro);

  auto* rows_widget = new QWidget;
  rows_widget->setObjectName(QStringLiteral("missingFontsRows"));
  auto* grid = new QGridLayout(rows_widget);
  grid->setColumnStretch(0, 1);
  struct Row {
    const MissingFontEntry* entry;
    QCheckBox* replace;
    FontPickerCombo* picker;
  };
  std::vector<Row> rows;
  auto* resolve_button = new QPushButton(QObject::tr("Resolve Fonts"), &dialog);
  resolve_button->setObjectName(QStringLiteral("missingFontsResolveButton"));
  const auto sync_resolve_button = [&rows, resolve_button] {
    resolve_button->setEnabled(std::ranges::any_of(rows, [](const Row& row) { return row.replace->isChecked(); }));
  };
  for (const auto& entry : fonts) {
    const auto index = static_cast<int>(rows.size());
    auto* name = new QLabel(entry.family, rows_widget);
    name->setObjectName(QStringLiteral("missingFontName%1").arg(index));
    name->setTextFormat(Qt::PlainText);
    name->setFont(offset_font(name->font(), 0, true));
    auto* detail = new QLabel(entry.no_glyphs
                                  ? QObject::tr("Installed, but has no glyphs for this text. Used in %n text layer(s).",
                                                nullptr, entry.layer_count)
                                  : QObject::tr("Not installed. Used in %n text layer(s).", nullptr, entry.layer_count),
                              rows_widget);
    detail->setObjectName(QStringLiteral("missingFontDetail"));
    detail->setWordWrap(true);
    auto* name_column = new QVBoxLayout;
    name_column->setSpacing(1);
    name_column->addWidget(name);
    name_column->addWidget(detail);
    grid->addLayout(name_column, index, 0);

    // Unchecked is Photoshop's "Don't Resolve": the file keeps naming the font, so a colleague
    // who has it still sees the original. The picker starts on the face drawing the text now.
    auto* replace = new QCheckBox(QObject::tr("Replace with:"), rows_widget);
    replace->setObjectName(QStringLiteral("missingFontReplaceCheck%1").arg(index));
    grid->addWidget(replace, index, 1, Qt::AlignVCenter);
    auto* picker = new FontPickerCombo(rows_widget);
    picker->setObjectName(QStringLiteral("missingFontReplacementCombo%1").arg(index));
    picker->setMinimumWidth(220);
    if (!entry.suggested.isEmpty()) {
      picker->setCurrentFont(QFont(entry.suggested));
    }
    picker->setEnabled(false);
    grid->addWidget(picker, index, 2, Qt::AlignVCenter);
    QObject::connect(replace, &QCheckBox::toggled, picker, &QWidget::setEnabled);
    QObject::connect(replace, &QCheckBox::toggled, &dialog, [&sync_resolve_button](bool) { sync_resolve_button(); });
    rows.push_back(Row{&entry, replace, picker});
  }

  auto* scroll = new QScrollArea(&dialog);
  scroll->setObjectName(QStringLiteral("missingFontsScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setWidget(rows_widget);
  // Up to five rows show without scrolling; a longer list scrolls inside the dialog.
  const auto row_height = std::max(1, rows_widget->sizeHint().height() / std::max<int>(1, static_cast<int>(rows.size())));
  scroll->setMinimumHeight(std::min(rows_widget->sizeHint().height(), row_height * 5) + 4);
  layout->addWidget(scroll, 1);

  auto* dont_show = new QCheckBox(QObject::tr("Don't show when opening documents"), &dialog);
  dont_show->setObjectName(QStringLiteral("missingFontsDontShowCheck"));
  dont_show->setToolTip(QObject::tr("Type > Resolve Missing Fonts opens this dialog at any time. Preferences can "
                                    "turn it back on for opened documents."));
  dont_show->setChecked(!show_on_open);
  layout->addWidget(dont_show);

  auto* buttons = new QDialogButtonBox(&dialog);
  buttons->addButton(resolve_button, QDialogButtonBox::AcceptRole);
  auto* cancel_button = buttons->addButton(QDialogButtonBox::Cancel);
  cancel_button->setObjectName(QStringLiteral("missingFontsCancelButton"));
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  resolve_button->setDefault(true);
  sync_resolve_button();

  append_themed_style(dialog, QStringLiteral("QLabel#missingFontDetail { color: @text_secondary; }"));
  dialog.resize(std::max(560, dialog.sizeHint().width()), dialog.sizeHint().height());
  remember_dialog_position(dialog);
  const auto accepted = exec_dialog(dialog) == QDialog::Accepted;
  if (show_on_open_out != nullptr) {
    *show_on_open_out = !dont_show->isChecked();
  }
  if (!accepted) {
    return std::nullopt;
  }
  MissingFontsDialogResult result;
  result.show_on_open = !dont_show->isChecked();
  for (const auto& row : rows) {
    if (!row.replace->isChecked()) {
      continue;
    }
    const auto chosen = row.picker->currentFont().family();
    if (!chosen.trimmed().isEmpty()) {
      result.replacements.insert(text_font_replacement_key(row.entry->family), chosen);
    }
  }
  return result;
}

}  // namespace patchy::ui
