#include "ui/recent_file_list.hpp"

#include "ui/dialog_utils.hpp"
#include "ui/theme_palette.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionViewItem>

#include <algorithm>

namespace patchy::ui {

QStringList recent_filter_tokens(const QString& filter_text) {
  return filter_text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

bool recent_path_matches(const QString& path, const QStringList& tokens) {
  const auto native_path = QDir::toNativeSeparators(path);
  return std::all_of(tokens.begin(), tokens.end(), [&native_path](const QString& token) {
    return native_path.contains(token, Qt::CaseInsensitive);
  });
}

RecentFileList::RecentFileList(QWidget* parent, int visible_rows)
    : QListWidget(parent), visible_rows_(std::max(1, visible_rows)) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  setMouseTracking(true);
  // The delegate elides both text lines to the row width, so only the vertical bar
  // is ever wanted; per-pixel scrolling keeps the wheel smooth over a long list.
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setUniformItemSizes(true);  // constant row height; skips a sizeHint per row
  setItemDelegate(new RecentFileDelegate(this));
  setContextMenuPolicy(Qt::CustomContextMenu);
  // On the viewport, not the list: the scroll bar keeps a normal arrow cursor.
  viewport()->setCursor(Qt::PointingHandCursor);
}

void RecentFileList::set_fixed_rows(int rows) {
  fixed_rows_ = std::max(0, rows);
  updateGeometry();
}

QSize RecentFileList::sizeHint() const {
  const auto rows = fixed_rows_ > 0 ? fixed_rows_ : count();
  return {QListWidget::sizeHint().width(), std::clamp(rows, 1, visible_rows_) * kRecentRowHeight + kRecentListChrome};
}

QSize RecentFileList::minimumSizeHint() const {
  return {QListWidget::minimumSizeHint().width(), kRecentRowHeight + kRecentListChrome};
}

QSize RecentFileDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
  return QSize(option.rect.width(), kRecentRowHeight);
}

void RecentFileDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  painter->save();
  const QRect row = option.rect.adjusted(2, 2, -2, -2);
  const auto path = index.data(kRecentPathRole).toString();
  if (path.isEmpty()) {
    // The no-match placeholder: one dimmed line, and no hover or selection
    // chrome, because there is nothing here to click.
    painter->setPen(theme().start_panel_muted_text);
    painter->drawText(row.adjusted(10, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());
    painter->restore();
    return;
  }

  const bool selected = (option.state & QStyle::State_Selected) != 0;
  const bool hovered = (option.state & QStyle::State_MouseOver) != 0;
  if (selected || hovered) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(selected ? theme().selection_soft_bg : theme().start_panel_row_hover_bg);
    painter->drawRoundedRect(row, 4.0, 4.0);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  const QFileInfo info(path);
  const QRect text_area = row.adjusted(10, 3, -10, -3);
  const auto name_font = offset_font(option.font, 0, true);
  painter->setFont(name_font);
  painter->setPen(theme().text_primary);
  const QRect name_rect(text_area.left(), text_area.top(), text_area.width(), text_area.height() / 2);
  painter->drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(name_font).elidedText(info.fileName(), Qt::ElideMiddle, name_rect.width()));

  const auto path_font = offset_font(option.font, -1, false);
  painter->setFont(path_font);
  painter->setPen(theme().start_panel_muted_text);
  const QRect path_rect(text_area.left(), text_area.top() + text_area.height() / 2, text_area.width(),
                        text_area.height() - text_area.height() / 2);
  painter->drawText(path_rect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(path_font).elidedText(QDir::toNativeSeparators(info.absolutePath()),
                                                       Qt::ElideMiddle, path_rect.width()));
  painter->restore();
}

int fill_recent_file_list(QListWidget& list, const QStringList& paths, const QString& filter_text,
                          const QString& placeholder) {
  list.clear();
  const auto tokens = recent_filter_tokens(filter_text);
  int rows = 0;
  for (const auto& path : paths) {
    if (!recent_path_matches(path, tokens)) {
      continue;
    }
    auto* item = new QListWidgetItem(QFileInfo(path).fileName(), &list);
    item->setData(kRecentPathRole, path);
    item->setToolTip(QDir::toNativeSeparators(path));
    ++rows;
  }
  if (!paths.isEmpty() && rows == 0) {
    // Keeps the box where it is while a filter hides every row, instead of
    // collapsing it under the edit the user is typing into.
    auto* item = new QListWidgetItem(placeholder, &list);
    item->setData(kRecentPathRole, QString());
    item->setFlags(Qt::NoItemFlags);
  }
  list.updateGeometry();
  return rows;
}

}  // namespace patchy::ui
