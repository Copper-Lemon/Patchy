#pragma once

// The Options bar's wrapping host (GitHub issue 91). FlowLayout packs the active
// tool's controls left to right and folds them onto more rows when the window is
// narrow; OptionsFlowContainer sizes the bar from every tool's row, not just the
// active one, so switching tools never changes the bar height (which would shift
// the tool palette, canvas and docks) and duplicate inline sliders drop out before
// anything wraps.

#include <QLayout>
#include <QList>
#include <QSet>
#include <QWidget>

#include <functional>
#include <vector>

class QResizeEvent;

namespace patchy::ui {

// Set to true on an Options-bar widget that duplicates a control beside it (the
// inline sliders next to a spin box that already has a popup slider and a scrub
// label). The bar hides every such widget at widths where a row that contains one
// would otherwise wrap; MainWindow::refresh_options_bar applies it.
inline constexpr char kOptionsBarCollapsibleProperty[] = "optionsBarCollapsible";

// A left-to-right layout that wraps its items onto additional rows when the
// available width is too small, skipping hidden widgets so the active tool's
// options pack tightly.
class FlowLayout final : public QLayout {
public:
  explicit FlowLayout(QWidget* parent, int horizontal_spacing = 6, int vertical_spacing = 4);
  ~FlowLayout() override;

  void addItem(QLayoutItem* item) override;
  int count() const override;
  QLayoutItem* itemAt(int index) const override;
  QLayoutItem* takeAt(int index) override;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int width) const override;
  void setGeometry(const QRect& rect) override;
  QSize sizeHint() const override;
  QSize minimumSize() const override;
  void invalidate() override;

  // Every tool's row: the widgets each tool shows, in no particular order (the
  // layout keeps its own item order). Hidden widgets are measured too.
  void set_rows(std::vector<QSet<QWidget*>> rows);
  [[nodiscard]] bool has_rows() const { return !rows_.empty(); }

  struct RowsMeasure {
    bool collapse = false;  // hide kOptionsBarCollapsibleProperty widgets at this width
    int height = 0;         // the tallest row once collapsed widgets are dropped
  };
  // Cached per width until the next invalidate() or set_rows().
  [[nodiscard]] RowsMeasure measure_rows(int width) const;

private:
  struct Flow {
    int height = 0;
    int lines = 0;
  };
  // include == nullptr lays out the widgets that are not hidden (the live row).
  Flow do_layout(const QRect& rect, bool test_only, const std::function<bool(QWidget*)>* include) const;

  QList<QLayoutItem*> items_;
  int horizontal_spacing_;
  int vertical_spacing_;
  std::vector<QSet<QWidget*>> rows_;
  mutable int cached_width_ = -1;
  mutable RowsMeasure cached_measure_;
};

// Hosts the Options bar controls in a FlowLayout and reports the height for its
// current width so the surrounding QToolBar grows to a second row. With rows
// set, that height is the tallest tool row at this width, so it depends on the
// window width alone and stays put across tool switches.
class OptionsFlowContainer final : public QWidget {
public:
  using QWidget::QWidget;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

  // Whether the duplicate sliders should hide at the current width.
  [[nodiscard]] bool wants_collapse() const;
  // MainWindow reports the collapse state it applied, and gets a callback when a
  // resize crosses the threshold so it can re-run its visibility pass.
  void set_collapse_applied(bool collapsed) { collapse_applied_ = collapsed; }
  [[nodiscard]] bool collapse_applied() const { return collapse_applied_; }
  void set_collapse_changed_callback(std::function<void()> callback) { collapse_changed_ = std::move(callback); }

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  [[nodiscard]] int height_for(int width) const;

  bool collapse_applied_ = false;
  std::function<void()> collapse_changed_;
};

}  // namespace patchy::ui
