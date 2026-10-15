# Wayland

Patchy behavior that differs on a Wayland session (Qt's `wayland` platform plug-in). X11
(`xcb`), Windows, macOS and the offscreen test platform take none of these paths. The
right-dock rule (panels stay in the column, GitHub issue 102) lives in
[right-docks.md](right-docks.md); the KWin and GNOME test rigs for dock drags are in
[testing.md](testing.md).

## Popup anchors at a non-100% interface scale (GitHub issue 100)

The interface scale is `QT_SCALE_FACTOR` (`apply_gui_scale_factor` in `src/app/main.cpp`).
On Wayland Qt cannot place a popup itself: it sends the compositor an `xdg_positioner`
whose anchor rectangle is in the parent surface's pixels, which at a scale factor are the
native pixels, not Qt's device-independent ones.

Qt 6.11 added semantic popup placement (qtbase "widgets: Pass popup semantic information
to Wayland", QTBUG-135883): `QMenuPrivate::popup` and `QComboBox::showPopup` hand the
plug-in a control rectangle through the private `setParentControlGeometry`, and
`QWaylandXdgSurface::createPositioner` uses it as the anchor rectangle. The rectangle is in
widget coordinates and nothing converts it, so every anchored popup is off by the scale
factor. Still unconverted in qtbase `dev` in October 2026, and no matching Qt bug report
was found. Affected:

| Popup | Anchor Qt sends | Symptom at 125% / 90% |
|---|---|---|
| Menu bar menu | the menu bar item | opens left of the item and over its bottom edge / right of the item, a gap below |
| Submenu (Image > Mode, File > Open Recent File) | the parent item, anchored at its right edge | overlaps the parent menu / a gap between the menus |
| QPushButton menu (the layers panel's adjustment button) | the button | like the menu bar case |
| Combo box list | the combo box | covers the combo box / a gap below it |

Unaffected: context menus (`QMenu::popup`/`exec` at a point) and QToolButton menus, which
set no control rectangle; Qt 6.8.3 (the AppImage, Windows, macOS), whose anchor is the
popup's own native position; X11.

Measured on Qt 6.11.2 with the probe below, 125%, a 159 px wide Image menu: the Mode
submenu opened at x=127 of its parent (should be 159) and the Image menu at (41, 19)
instead of (51, 24). At 90%, a 114 px menu: submenu at 127 (should be 114). A combo box
whose bottom edge is at y=104 got its list at y=83 at 125%. Patchy 1.11 (the Flatpak) at
125%: Image > Mode at x=332 of a 416 px Image menu; at 90%, x=332 of a 300 px menu. With
the workaround the probe matches the X11 placement and Patchy opens Mode at the menu's
right edge (415 and 299).

### The workaround

`install_wayland_popup_anchor_fix()` (`src/ui/wayland_popup_anchor.cpp`, called from
`main()`) installs an application event filter when `wayland_popup_anchor_fix_needed()`
holds: a `wayland*` platform, Qt 6.11.x, and `QT_SCALE_FACTOR` other than 1. On each popup's
Show event, which arrives before the platform window becomes visible and the plug-in builds
the positioner, `wayland_popup_control_rect()` recomputes the rectangle Qt sent (same
cases and order as `QMenuPrivate::popup`: a pressed QPushButton showing the menu, then a
visible parent menu whose active action is this menu, then the menu bar's active item;
combo lists through `QStyle::SC_ComboBoxListBoxPopup`) and the filter stores it, scaled,
in the window property `_q_waylandPopupAnchorRect`. The plug-in reads that property after
the control rectangle and uses it as is; anchor, gravity and constraint adjustments still
come from Qt's popup type. A popup Qt anchors to nothing gets the property cleared, so a
menu reopened as a context menu does not keep a stale anchor.

Keep the version gate exact. If a later Qt converts the control rectangle, the property is
still right, but if it starts converting the property, a gate that included it would
scale twice. When the Flatpak runtime moves past 6.11, run the probe without `fix`: if Qt
still places popups off by the scale, extend the gate to that version; otherwise leave it.

`ui_wayland_popup_anchor_fix_scales_qt_anchor_rects` drives the filter offscreen (menu bar
menu, submenu, the same menu as a context menu, combo list, QPushButton menu) and checks
the property against the rectangles Qt computes. Offscreen cannot show the compositor's
placement; the probe does.

## A window taller than the screen (GitHub issue 108)

Qt's `QScreen::availableGeometry` on Wayland is the whole output: no protocol reports a
panel, and an app cannot move its window either, so `clamp_window_to_available_screen`
could neither shrink the window below its minimum height nor nudge it onto the screen. On
a 1366x768 laptop at a 125% scale the main window's minimum (the right column's 300 px
Layers floor plus the collapsed strips, two options-bar rows, menu and status bars) was
taller than the screen: the column's bottom panels sat under GNOME's top bar and off the
bottom edge, and expanding Properties or Info demanded more height and grew the window
further out of view, which the reporter saw as cropped panels.

`MainWindow::available_window_height` is the client height that keeps the whole frame on
the screen: the available height less the decorations outside the client area (Qt's own
client-side title bar on GNOME, known only once the window is shown, hence the deferred
first-show clamp on Wayland in `showEvent`) less `kWaylandShellPanelAllowance` (48 logical
px) for the panel the compositor never reports. `fit_right_dock_floors_to_screen` lowers
the expanded docks' explicit floors (the dock just expanded first, then Layers, including
the Layers panel's own 240 px and the list's 120 px, then the rest of the column) until the
window's minimum fits that budget; `clamp_window_to_available_screen` and
`handle_right_dock_panel_toggled` call it, so an expand takes room from the column while
the column has any to give. Qt re-derives a dock's minimum from its content at every layout
activation (`QDockWidgetLayout` runs with `SetMinAndMaxSize`), so the floors that can yield
are the explicit ones on the panels; once every panel is open, the content minimums alone
(Info's four lines, the Properties scroll area) can still exceed a 1366x768 screen at 125%,
and then the window grows to that irreducible minimum as before. Platform-neutral (a short
Windows screen gets the same floors); only the panel allowance is Wayland-specific.
`ui_right_dock_floors_fit_the_screen_height` covers it on the offscreen platform's 600 px
screen. See [right-docks.md](right-docks.md).

## Reproducing on a headless GNOME Shell

`scripts/dev/wayland-gnome/gnome-rig.sh` runs the installed Flatpak on a headless mutter
(`gnome-shell --headless --wayland --no-x11 --unsafe-mode --virtual-monitor <MON>`, its own
session bus, env in `build/gnome-rig/env`), the setup the issue 100 and 108 reporter has (Qt
draws its own title bar there). `start-shell` (with `MON=1366x768` for that laptop), then
`patchy <scale>` launches the Flatpak as the single instance, `script <file.js>` forwards a
`--run-script` to it (the script file must live under `$HOME`: the sandbox hides `/tmp`),
and `drive` reads pointer commands on stdin (`gnome-input.py`: `move X Y` in output pixels,
`press`, `release`, `click`, `sleep MS`, `shot NAME` through `org.gnome.Shell.Screenshot`).
The pointer is `org.gnome.Mutter.RemoteDesktop` relative motion anchored at the bottom-left
corner, because the top-left one is the Activities hot corner and opens the overview; if
the overview is up anyway, `gdbus call ... org.gnome.Shell.Eval 'Main.overview.hide()'`.
`stop` kills Patchy by Flatpak id and the shell by the pids it recorded; never by name.

The sway rig is faster for anything that needs no GNOME specifics: `run.sh` with `KEEP=1`
leaves the container up and `scripts/dev/wayland-popups/run-patchy.sh` launches, scripts,
clicks (`vptr` now has `press`/`release` for drags) and captures the Flatpak on it. Both rigs
share the single-instance socket: stop one Patchy before starting the next, or the launch
only forwards to the running one.

## Measuring popup placement headlessly

`scripts/dev/wayland-popups/run.sh <scale> [menu|combo] [fix]` runs on the Linux build
host (podman plus the user-installed Flatpak SDK) and never touches a desktop session. It
builds `popup_probe` (a menu bar with Image > Mode and a combo box, linked with
`wayland_popup_anchor.cpp`; `fix` turns the workaround on) and a virtual pointer client
inside the manifest's `org.kde.Sdk`, starts a headless sway in a Debian 12 container on its
own socket in `$XDG_RUNTIME_DIR` (`--userns=keep-id`, so the socket belongs to the user),
clicks the target, and prints Qt's view of each popup, the anchors Qt sent
(`xdg_positioner.set_anchor_rect`) and the positions the compositor chose
(`xdg_popup.configure`, relative to the parent surface). Screenshots (grim) and the full
`WAYLAND_DEBUG` log land in `build/wayland-popups/`. `KEEP=1` keeps sway for the next run.

Why this setup:

- Qt makes a grabbing popup only after real input on the parent: Qt 6.8.3 otherwise maps
  menus as toplevels, and 6.11 logs "Failed to create grabbing popup". weston's headless
  backend has no seat, and wtype key presses did not count for 6.11. sway accepts a
  `zwlr_virtual_pointer_v1` device, which gives the seat a pointer and the click a serial;
  `vptr.c` keeps the device alive until its input ends.
- sway refuses to run as root (`--userns=keep-id` plus `setpriv`) and refuses an NVIDIA host
  without `--unsupported-gpu`; with no GPU in the container it renders with pixman.
- For a Qt 6.8.3 comparison, build the probe directory against `.deps/Qt/6.8.3/gcc_64` on
  the host and run it with `WAYLAND_DISPLAY` set to the sway socket (`build/wayland-popups/display`).
