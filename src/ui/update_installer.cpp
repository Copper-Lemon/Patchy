#include "ui/update_installer.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#endif

namespace patchy::ui {

bool install_location_matches(const QString& registered_install_location, const QString& application_dir) {
  const auto normalized = [](const QString& path) {
    // Not fromNativeSeparators: these are Windows paths on every platform that compares them.
    return QDir::cleanPath(path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/')));
  };
  const auto registered = normalized(registered_install_location);
  const auto application = normalized(application_dir);
  if (registered_install_location.trimmed().isEmpty() || application_dir.trimmed().isEmpty()) {
    return false;
  }
  return registered.compare(application, Qt::CaseInsensitive) == 0;
}

bool installed_copy_can_self_update() {
#ifdef Q_OS_WIN
  // The entry packaging/windows/InstallPatchy.ps1 writes; the portable zip registers nothing.
  const QSettings uninstall_entry(
      QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Patchy"),
      QSettings::NativeFormat);
  const auto application_dir = QCoreApplication::applicationDirPath();
  return install_location_matches(uninstall_entry.value(QStringLiteral("InstallLocation")).toString(),
                                  application_dir) &&
         QFileInfo(QDir(application_dir).filePath(QStringLiteral("UninstallPatchy.exe"))).isFile();
#else
  return false;
#endif
}

QString update_installer_file_name() {
  return QStringLiteral("PatchyWindowsInstaller.exe");
}

QString update_download_folder(const QString& version) {
  QString safe_version;
  for (const auto ch : version) {
    if (ch.isDigit() || ch == QLatin1Char('.')) {
      safe_version.append(ch);
    }
  }
  return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
      .filePath(QStringLiteral("patchy-update-") + safe_version);
}

void remove_update_download(const QString& version) {
  const auto folder = update_download_folder(version);
  if (!QFileInfo(folder).isDir()) {
    return;
  }
  QFile::remove(QDir(folder).filePath(update_installer_file_name()));
  QDir().rmdir(folder);  // only when that left it empty
}

bool launch_update_installer(const UpdateInstallerLaunch& launch) {
#ifdef Q_OS_WIN
  const QFileInfo installer(launch.installer_path);
  if (!installer.isFile() || launch.install_dir.isEmpty()) {
    return false;
  }
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("PATCHY_UPDATE_INSTALL_DIR"), QDir::toNativeSeparators(launch.install_dir));
  environment.insert(QStringLiteral("PATCHY_UPDATE_WAIT_PID"), QString::number(launch.wait_process_id));
  QProcess process;
  process.setProgram(installer.absoluteFilePath());
  process.setWorkingDirectory(installer.absolutePath());
  process.setProcessEnvironment(environment);
  return process.startDetached();
#else
  Q_UNUSED(launch);
  return false;
#endif
}

}  // namespace patchy::ui
