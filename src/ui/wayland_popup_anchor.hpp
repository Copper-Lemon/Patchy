#pragma once

#include <QRect>
#include <QString>
#include <QVersionNumber>

#include <optional>

class QObject;
class QWidget;

namespace patchy::ui {

// Workaround for a Qt 6.11 Wayland bug behind GitHub issue 100 (docs/wayland.md). Qt 6.11
// hands the Wayland plug-in an anchor rectangle for menu bar menus, submenus, QPushButton
// menus and combo box lists in device-independent pixels, and the plug-in sends it to the
// compositor unconverted as the xdg_positioner anchor, which is in surface pixels. With an
// interface scale other than 100% (QT_SCALE_FACTOR) a submenu then opens with a gap (90%)
// or over its parent menu (125%), and a menu bar menu misses its item. The workaround sets
// Qt's own `_q_waylandPopupAnchorRect` window property, which overrides that anchor, to the
// same rectangle in surface pixels just before the popup's surface is created.

// Whether the workaround applies: a Wayland platform, Qt 6.11.x (the bug arrived in 6.11.0;
// re-verify before widening the range, a fixed Qt would scale the property twice), and a
// scale factor other than 1.
[[nodiscard]] bool wayland_popup_anchor_fix_needed(const QString& platform_name,
                                                   const QVersionNumber& qt_version,
                                                   double scale_factor);

// The anchor Qt 6.11 computes for `popup` in device-independent pixels, relative to the
// window it opens from, or nullopt when Qt sets none (context menus, QToolButton menus,
// other popups), where Qt's own placement is already right. Mirrors QMenuPrivate::popup
// and QComboBox::showPopup; call it while the popup is being shown.
[[nodiscard]] std::optional<QRect> wayland_popup_control_rect(const QWidget& popup);

// `logical` in native pixels at `scale_factor`, rounded the way QHighDpi scales a rect.
[[nodiscard]] QRect scale_popup_anchor_rect(const QRect& logical, double scale_factor);

// An event filter that applies the workaround at `scale_factor` to every popup shown,
// whatever the platform (tests drive it offscreen). Owned by `parent`.
QObject* create_wayland_popup_anchor_filter(double scale_factor, QObject* parent);

// Installs the filter on the application when wayland_popup_anchor_fix_needed() holds for
// the running platform, Qt and QT_SCALE_FACTOR. Call after the QApplication exists.
void install_wayland_popup_anchor_fix();

}  // namespace patchy::ui
