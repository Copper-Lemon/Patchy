#pragma once

// The two-line recent list shared by the start panel's Recent Files box and the Open
// Recent File / Folder menus (docs/ui-conventions.md): name over its dimmed folder,
// both elided in the middle, a fixed row height, and a box that scrolls instead of
// growing past its visible row cap.

#include <QListWidget>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>

namespace patchy::ui {

// The full path of a recent row. An empty path marks the "no matches" placeholder row,
// which the delegate, click and context-menu handlers all test for; never give a real
// row an empty path.
inline constexpr int kRecentPathRole = Qt::UserRole + 1;
inline constexpr int kRecentRowHeight = 40;
inline constexpr int kRecentListChrome = 14;  // the list's own frame and padding

// The shared filter rule: every whitespace-separated token of `filter_text`, anywhere in
// the native path, case-insensitive, so a folder name narrows as well as a file name.
[[nodiscard]] QStringList recent_filter_tokens(const QString& filter_text);
[[nodiscard]] bool recent_path_matches(const QString& path, const QStringList& tokens);

// A list as tall as its rows (or `fixed_rows` when set), capped at `visible_rows`;
// longer lists scroll. setFixedHeight cannot express that: the application style sheet
// re-polishes the widget and drops the minimum that call sets. A real sizeHint survives
// the re-polish, and the Maximum policy lets a short window shrink the box.
class RecentFileList final : public QListWidget {
 public:
  RecentFileList(QWidget* parent, int visible_rows);

  // Rows the height is computed from instead of count(); 0 follows count(). The Open
  // Recent menus pin this to the unfiltered list so filtering never resizes the menu.
  void set_fixed_rows(int rows);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 private:
  int visible_rows_;
  int fixed_rows_{0};
};

// Two-line recent row: file name over its dimmed directory, with hover and selection
// chrome. The placeholder row (empty kRecentPathRole) draws one dimmed line only.
class RecentFileDelegate final : public QStyledItemDelegate {
 public:
  explicit RecentFileDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

// Refills `list` with the `paths` matching `filter_text`, or with one `placeholder` row
// when paths exist but none match. Returns the number of real rows.
int fill_recent_file_list(QListWidget& list, const QStringList& paths, const QString& filter_text,
                          const QString& placeholder);

}  // namespace patchy::ui
