#include "ui/tool_palette_footer.hpp"

#include "ui/theme_palette.hpp"

#include <QAction>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace patchy::ui {

namespace {

constexpr int kSwatchSize = 22;
constexpr int kGlyphSize = 14;

QColor ink_for(const QWidget& widget) {
  const auto& palette = theme();
  if (!widget.isEnabled()) {
    QColor dim = palette.icon_ink;
    dim.setAlphaF(0.4F);
    return dim;
  }
  return widget.underMouse() ? palette.accent_bright : palette.icon_ink;
}

}  // namespace

ColorWellSwatch::ColorWellSwatch(QWidget* parent) : QPushButton(parent) {
  setFlat(true);
  setFocusPolicy(Qt::NoFocus);
  setCursor(Qt::PointingHandCursor);
  setFixedSize(kSwatchSize, kSwatchSize);
  setAttribute(Qt::WA_Hover, true);
}

void ColorWellSwatch::set_color(QColor color) {
  if (color_ == color) {
    return;
  }
  color_ = color;
  update();
}

QSize ColorWellSwatch::sizeHint() const {
  return {kSwatchSize, kSwatchSize};
}

void ColorWellSwatch::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  const QRect box = rect().adjusted(0, 0, -1, -1);
  QColor fill = color_;
  if (!isEnabled()) {
    fill.setAlphaF(0.5F);
  }
  painter.fillRect(box, fill);
  // A hairline of the opposite tone inside the ink border keeps a swatch that
  // matches the palette background visible.
  painter.setPen(QPen(color_.lightness() < 128 ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 60), 1.0));
  painter.drawRect(box.adjusted(1, 1, -1, -1));
  painter.setPen(QPen(ink_for(*this), 1.0));
  painter.drawRect(box);
}

ColorWellGlyphButton::ColorWellGlyphButton(Glyph glyph, QWidget* parent) : QToolButton(parent), glyph_(glyph) {
  setAutoRaise(true);
  setFocusPolicy(Qt::NoFocus);
  setCursor(Qt::PointingHandCursor);
  setFixedSize(kGlyphSize, kGlyphSize);
  setAttribute(Qt::WA_Hover, true);
}

QSize ColorWellGlyphButton::sizeHint() const {
  return {kGlyphSize, kGlyphSize};
}

void ColorWellGlyphButton::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QColor ink = ink_for(*this);
  if (glyph_ == Glyph::Default) {
    // A tiny black-over-white pair: the default colors.
    painter.setPen(QPen(ink, 1.0));
    painter.setBrush(Qt::white);
    painter.drawRect(QRectF(5.5, 5.5, 7.0, 7.0));
    painter.setBrush(Qt::black);
    painter.drawRect(QRectF(1.5, 1.5, 7.0, 7.0));
    return;
  }
  // Two short arrows bending between the two swatches' corners.
  QPen pen(ink, 1.3);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  QPainterPath arc;
  arc.moveTo(3.0, 10.5);
  arc.quadTo(3.0, 3.5, 10.5, 3.5);
  painter.drawPath(arc);
  painter.drawLine(QPointF(10.5, 3.5), QPointF(8.0, 1.5));
  painter.drawLine(QPointF(10.5, 3.5), QPointF(8.0, 5.5));
  painter.drawLine(QPointF(3.0, 10.5), QPointF(1.0, 8.0));
  painter.drawLine(QPointF(3.0, 10.5), QPointF(5.0, 8.0));
}

ToolPaletteFooter::ToolPaletteFooter(QAction* default_colors, QAction* swap_colors, QAction* quick_mask,
                                     QWidget* parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("toolPaletteFooter"));
  setFixedHeight(kHeight);
  setMinimumWidth(kWidth);

  // The well: background square to the lower right, foreground square over
  // it, swap glyph in the free upper-right corner, default glyph in the free
  // lower-left corner. Everything is placed by hand inside a 43 x 42 cell.
  background_ = new ColorWellSwatch(this);
  background_->setObjectName(QStringLiteral("backgroundColorButton"));
  background_->setText(QStringLiteral("BG"));
  // Qt places the toolbar's extension button 2 px into the bottom margin, so
  // the well starts 3 px down.
  background_->move(17, 19);
  foreground_ = new ColorWellSwatch(this);
  foreground_->setObjectName(QStringLiteral("foregroundColorButton"));
  foreground_->setText(QStringLiteral("FG"));
  foreground_->move(4, 7);
  foreground_->raise();

  swap_ = new ColorWellGlyphButton(ColorWellGlyphButton::Glyph::Swap, this);
  swap_->setObjectName(QStringLiteral("colorSwapButton"));
  swap_->setDefaultAction(swap_colors);
  swap_->move(28, 4);
  default_ = new ColorWellGlyphButton(ColorWellGlyphButton::Glyph::Default, this);
  default_->setObjectName(QStringLiteral("colorDefaultButton"));
  default_->setDefaultAction(default_colors);
  default_->move(1, 30);

  quick_mask_ = new QToolButton(this);
  quick_mask_->setObjectName(QStringLiteral("quickMaskButton"));
  quick_mask_->setDefaultAction(quick_mask);
  quick_mask_->setAutoRaise(true);
  quick_mask_->move(7, kWellHeight + 3);
}

}  // namespace patchy::ui
