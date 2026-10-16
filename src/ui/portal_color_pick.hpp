#pragma once

// "Pick Screen Color" through the XDG desktop portal (Linux, compiled only when
// Qt D-Bus is available; PATCHY_HAVE_QT_DBUS). A Wayland compositor refuses
// QScreen::grabWindow and keeps a transparent overlay from covering other
// applications' windows, so the color picker's own eyedropper sampled nothing
// there (GitHub issue 128). org.freedesktop.portal.Screenshot.PickColor runs the
// compositor's own picker (GNOME shows a magnifier) and answers with the chosen
// sRGB color through the request's Response signal. docs/wayland.md.

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace patchy::ui {

// True on a Wayland session, where the overlay-and-grab picker cannot work.
[[nodiscard]] bool portal_color_pick_preferred();

// The object path the portal uses for a request this connection makes with
// `handle_token`: "/org/freedesktop/portal/desktop/request/<sender>/<token>",
// the sender being the connection's unique name without the leading colon and
// with dots replaced by underscores (":1.42" -> "1_42"). Connecting to that
// path before the call is how a client never misses the Response.
[[nodiscard]] QString portal_request_path(const QString& unique_bus_name, const QString& handle_token);

// Starts a PickColor request. `done` runs on the GUI thread, once, with the
// picked color or nullopt when the user cancelled or the portal failed. The
// callback is dropped when `context` is destroyed first. Returns false when
// the request could not even be sent (no session bus); the caller then falls
// back to its own picker.
bool request_portal_color_pick(QObject* context, std::function<void(std::optional<QColor>)> done);

// Receives the one Response of a PickColor request and reports it. Owned by
// the caller's context object so a destroyed picker drops the callback. Public
// only because QDBusConnection::connect needs a moc'd slot.
class PortalColorPickRequest final : public QObject {
  Q_OBJECT

public:
  PortalColorPickRequest(QObject* context, std::function<void(std::optional<QColor>)> done);

  void finish(std::optional<QColor> color);

public slots:
  // org.freedesktop.portal.Request.Response(u response, a{sv} results):
  // 0 is success, 1 the user cancelled, 2 another failure. The color is a
  // (ddd) struct of sRGB components in 0..1.
  void response(uint code, const QVariantMap& results);

private:
  std::function<void(std::optional<QColor>)> done_;
  bool finished_{false};
};

}  // namespace patchy::ui
