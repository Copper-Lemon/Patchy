#include "ui/connector_freshness.hpp"
#include "ui/appimage_environment.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <utility>

namespace patchy::ui {

ReplacedFileWatch::ReplacedFileWatch(QString path) : path_(std::move(path)) {
  const QFileInfo info(path_);
  existed_ = info.isFile();
  if (existed_) {
    size_ = info.size();
    modified_ = info.lastModified();
  }
}

bool ReplacedFileWatch::replaced() const {
  if (!existed_) {
    return false;
  }
  const QFileInfo info(path_);
  return !info.isFile() || info.size() != size_ || info.lastModified() != modified_;
}

namespace {

const ReplacedFileWatch& running_executable_watch() {
  // An AppImage's own binary sits in a read-only mount that never changes; updating
  // means replacing the .AppImage file.
  static const ReplacedFileWatch watch(user_visible_executable_path());
  return watch;
}

}  // namespace

void watch_running_executable() {
  (void)running_executable_watch();
}

QString connector_update_notice() {
  if (!running_executable_watch().replaced()) {
    return {};
  }
  return QStringLiteral("Patchy was updated after this connection started. This connection still runs version %1. "
                        "Tell the user to restart this AI app, or start a new session, to use the new version; "
                        "help text and behavior may not match until then.")
      .arg(QCoreApplication::applicationVersion());
}

bool add_connector_update_notice(QJsonObject& structured_content, QJsonArray& content) {
  const auto notice = connector_update_notice();
  if (notice.isEmpty()) {
    return false;
  }
  structured_content.insert(QStringLiteral("updateNotice"), notice);
  content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), notice}});
  return true;
}

namespace {

QString& update_available_text() {
  static QString text;  // UI thread only: set by the check's callback, read by get_info
  return text;
}

}  // namespace

QString connector_update_available_message(const QString& new_version) {
  // Nothing from the manifest but the version (digits and dots) goes into text an assistant
  // reads as instructions; the download address is fixed here.
  return QStringLiteral("Patchy %1 is available; this connection runs %2. Tell the user. To update, they open the "
                        "Patchy app, which offers the update when it starts, or download it from "
                        "https://github.com/SethRobinson/Patchy/releases/latest. Afterwards they restart this AI "
                        "app, or start a new session, so it uses the new version.")
      .arg(new_version, QCoreApplication::applicationVersion());
}

void set_connector_update_available(const QString& message) {
  update_available_text() = message;
}

QString connector_update_available() {
  return update_available_text();
}

}  // namespace patchy::ui
