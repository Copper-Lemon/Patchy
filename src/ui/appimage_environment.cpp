#include "ui/appimage_environment.hpp"

#include <QCoreApplication>
#include <QDir>

namespace patchy::ui {

QString appimage_file_path() {
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
    return {};
  }
  const auto appimage = qEnvironmentVariable("APPIMAGE");
  const auto appdir = qEnvironmentVariable("APPDIR");
  if (appimage.isEmpty() || appdir.isEmpty() || !QDir::isAbsolutePath(appimage)) {
    return {};
  }
  const auto mount = QDir(appdir).canonicalPath();
  const auto executable = QDir(QCoreApplication::applicationDirPath()).canonicalPath();
  if (mount.isEmpty() || !executable.startsWith(mount + QLatin1Char('/'))) {
    return {};
  }
  return QDir::cleanPath(appimage);
#else
  return {};
#endif
}

QString user_visible_executable_path() {
  const auto appimage = appimage_file_path();
  return appimage.isEmpty() ? QCoreApplication::applicationFilePath() : appimage;
}

}  // namespace patchy::ui
