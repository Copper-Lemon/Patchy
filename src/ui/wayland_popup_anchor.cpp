#include "ui/wayland_popup_anchor.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QVariant>
#include <QWindow>

#include <cmath>

namespace patchy::ui {
namespace {

// Read by Qt's xdg-shell plug-in when it builds the popup's xdg_positioner; it replaces
// the anchor rectangle Qt computed, unconverted, so it must already be in surface pixels.
constexpr char kAnchorRectProperty[] = "_q_waylandPopupAnchorRect";

QPushButton* menu_button_showing(const QMenu& menu) {
  // QPushButton::showMenu presses the button and then opens its menu; Qt 6.11 anchors
  // the menu to that button (QMenuPrivate::popup's causedButton).
  for (QWidget* widget : QApplication::allWidgets()) {
    auto* button = qobject_cast<QPushButton*>(widget);
    if (button != nullptr && button->menu() == &menu && button->isDown()) {
      return button;
    }
  }
  return nullptr;
}

std::optional<QRect> menu_control_rect(const QMenu& menu) {
  if (const QPushButton* button = menu_button_showing(menu)) {
    return QRect(button->mapTo(button->window(), QPoint(0, 0)), button->size());
  }
  QAction* menu_action = menu.menuAction();
  const auto owners = menu_action->associatedObjects();
  // A submenu: the visible menu whose active item is this menu's action.
  for (QObject* owner : owners) {
    auto* parent_menu = qobject_cast<QMenu*>(owner);
    if (parent_menu != nullptr && parent_menu != &menu && parent_menu->isVisible() &&
        parent_menu->activeAction() == menu_action) {
      return parent_menu->actionGeometry(menu_action);
    }
  }
  for (QObject* owner : owners) {
    auto* menu_bar = qobject_cast<QMenuBar*>(owner);
    if (menu_bar != nullptr && menu_bar->activeAction() == menu_action) {
      const QPoint bar_origin = menu_bar->mapTo(menu_bar->window(), QPoint(0, 0));
      return menu_bar->actionGeometry(menu_action).translated(bar_origin);
    }
  }
  return std::nullopt;
}

std::optional<QRect> combo_control_rect(const QComboBox& combo) {
  // QComboBox::initStyleOption is protected; the list-box popup rectangle depends only on
  // the widget rect and these fields in every style Patchy uses.
  QStyleOptionComboBox option;
  option.initFrom(&combo);
  option.editable = combo.isEditable();
  option.frame = combo.hasFrame();
  const QRect popup =
      combo.style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxListBoxPopup, &combo);
  return QRect(combo.mapTo(combo.window(), popup.topLeft()), popup.size());
}

class WaylandPopupAnchorFilter : public QObject {
 public:
  WaylandPopupAnchorFilter(double scale_factor, QObject* parent) : QObject(parent), scale_factor_(scale_factor) {}

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    // The Show event reaches the widget before its platform window becomes visible,
    // which is when the plug-in creates the popup surface and reads the property.
    if (event->type() != QEvent::Show) {
      return false;
    }
    auto* widget = qobject_cast<QWidget*>(watched);
    if (widget == nullptr || !widget->isWindow() || widget->windowType() != Qt::Popup) {
      return false;
    }
    QWindow* window = widget->windowHandle();
    if (window == nullptr) {
      return false;
    }
    if (const auto rect = wayland_popup_control_rect(*widget)) {
      window->setProperty(kAnchorRectProperty, scale_popup_anchor_rect(*rect, scale_factor_));
    } else if (window->property(kAnchorRectProperty).isValid()) {
      // The same menu reopened as a context menu: drop the anchor of its last showing.
      window->setProperty(kAnchorRectProperty, QVariant());
    }
    return false;
  }

 private:
  double scale_factor_;
};

}  // namespace

bool wayland_popup_anchor_fix_needed(const QString& platform_name, const QVersionNumber& qt_version,
                                     double scale_factor) {
  return platform_name.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive) &&
         qt_version >= QVersionNumber(6, 11) && qt_version < QVersionNumber(6, 12) &&
         std::abs(scale_factor - 1.0) > 1e-6;
}

std::optional<QRect> wayland_popup_control_rect(const QWidget& popup) {
  if (const auto* menu = qobject_cast<const QMenu*>(&popup)) {
    return menu_control_rect(*menu);
  }
  // The combo box list is Qt's private popup container, parented to its QComboBox.
  if (popup.inherits("QComboBoxPrivateContainer")) {
    if (const auto* combo = qobject_cast<const QComboBox*>(popup.parentWidget())) {
      return combo_control_rect(*combo);
    }
  }
  return std::nullopt;
}

QRect scale_popup_anchor_rect(const QRect& logical, double scale_factor) {
  const auto scale = [scale_factor](int value) { return static_cast<int>(std::lround(value * scale_factor)); };
  return QRect(scale(logical.x()), scale(logical.y()), scale(logical.width()), scale(logical.height()));
}

QObject* create_wayland_popup_anchor_filter(double scale_factor, QObject* parent) {
  return new WaylandPopupAnchorFilter(scale_factor, parent);
}

void install_wayland_popup_anchor_fix() {
  auto* app = qApp;
  if (app == nullptr) {
    return;
  }
  // Qt reads the same variable once at startup (main.cpp's apply_gui_scale_factor sets it).
  bool ok = false;
  const double scale_factor = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(&ok);
  if (!ok || scale_factor <= 0.0 ||
      !wayland_popup_anchor_fix_needed(QGuiApplication::platformName(), QLibraryInfo::version(), scale_factor)) {
    return;
  }
  app->installEventFilter(create_wayland_popup_anchor_filter(scale_factor, app));
}

}  // namespace patchy::ui
