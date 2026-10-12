#include "ui/update_installer.hpp"

#include "ui/qt_paths.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <array>
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

namespace {

QString updates_root() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
      .filePath(QStringLiteral("updates"));
}

}  // namespace

QString update_download_folder(const QString& version) {
  QString safe_version;
  for (const auto ch : version) {
    if (ch.isDigit() || ch == QLatin1Char('.')) {
      safe_version.append(ch);
    }
  }
  return QDir(updates_root()).filePath(QStringLiteral("patchy-update-") + safe_version);
}

void remove_update_download(const QString& version) {
  const auto folder = update_download_folder(version);
  if (!QFileInfo(folder).isDir()) {
    return;
  }
  QFile::remove(QDir(folder).filePath(update_installer_file_name()));
  QDir().rmdir(folder);          // only when that left it empty
  QDir().rmdir(updates_root());  // likewise
}

bool file_matches_checksum(const QString& path, qint64 size, const QByteArray& sha256_hex) {
  QFile file(path);
  if (size <= 0 || sha256_hex.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() != size) {
    return false;
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  return hash.addData(&file) && hash.result().toHex() == sha256_hex.toLower();
}

bool installer_signer_acceptable(const FileSignature& running_app, const FileSignature& installer) {
  if (!running_app.valid || running_app.signer.isEmpty()) {
    return true;
  }
  return installer.valid && installer.signer == running_app.signer;
}

FileSignature file_signature(const QString& path) {
  FileSignature result;
#ifdef Q_OS_WIN
  const auto native_path = to_filesystem_path(path);
  WINTRUST_FILE_INFO file_info{};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = native_path.c_str();
  WINTRUST_DATA trust{};
  trust.cbStruct = sizeof(trust);
  trust.dwUIChoice = WTD_UI_NONE;
  // No revocation lookup and nothing fetched: a quit or an update must never wait on the
  // network (the same reason host lookups run on a detached thread).
  trust.fdwRevocationChecks = WTD_REVOKE_NONE;
  trust.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  trust.dwUnionChoice = WTD_CHOICE_FILE;
  trust.pFile = &file_info;
  trust.dwStateAction = WTD_STATEACTION_VERIFY;
  GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  const auto no_window = static_cast<HWND>(INVALID_HANDLE_VALUE);
  if (WinVerifyTrust(no_window, &action, &trust) == ERROR_SUCCESS) {
    auto* provider = WTHelperProvDataFromStateData(trust.hWVTStateData);
    auto* signer = provider != nullptr ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
    auto* certificate = signer != nullptr ? WTHelperGetProvCertFromChain(signer, 0) : nullptr;
    if (certificate != nullptr && certificate->pCert != nullptr) {
      std::array<wchar_t, 512> name{};
      const auto length = CertGetNameStringW(certificate->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr,
                                             name.data(), static_cast<DWORD>(name.size()));
      if (length > 1) {
        result.signer = QString::fromWCharArray(name.data(), static_cast<int>(length) - 1);
        result.valid = true;
      }
    }
  }
  trust.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(no_window, &action, &trust);
#else
  Q_UNUSED(path);
#endif
  return result;
}

UpdateLaunchResult launch_update_installer(const UpdateInstallerLaunch& launch) {
#ifdef Q_OS_WIN
  const QFileInfo installer(launch.installer_path);
  if (!installer.isFile() || launch.install_dir.isEmpty()) {
    return {UpdateLaunchFailure::InstallerMissing};
  }
  const auto installer_path = installer.absoluteFilePath();
  // Held open without write or delete sharing from here until the process exists: the save
  // prompts may have taken minutes since the download was checked, and nothing may swap the
  // file between this check and its start.
  const auto native_path = to_filesystem_path(installer_path);
  const HANDLE guard = CreateFileW(native_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
  if (guard == INVALID_HANDLE_VALUE) {
    return {UpdateLaunchFailure::InstallerMissing};
  }
  static const FileSignature own_signature = file_signature(QCoreApplication::applicationFilePath());
  UpdateLaunchResult result;
  if (!file_matches_checksum(installer_path, launch.expected_size, launch.expected_sha256)) {
    result.failure = UpdateLaunchFailure::ChecksumMismatch;
  } else if (const auto signature = file_signature(installer_path);
             !installer_signer_acceptable(own_signature, signature)) {
    result.failure = signature.valid ? UpdateLaunchFailure::WrongSigner : UpdateLaunchFailure::NotSigned;
    result.expected_signer = own_signature.signer;
    result.installer_signer = signature.signer;
  } else {
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATCHY_UPDATE_INSTALL_DIR"), QDir::toNativeSeparators(launch.install_dir));
    environment.insert(QStringLiteral("PATCHY_UPDATE_WAIT_PID"), QString::number(launch.wait_process_id));
    QProcess process;
    process.setProgram(installer_path);
    process.setWorkingDirectory(installer.absolutePath());
    process.setProcessEnvironment(environment);
    if (!process.startDetached()) {
      result.failure = UpdateLaunchFailure::StartFailed;
    }
  }
  CloseHandle(guard);
  return result;
#else
  Q_UNUSED(launch);
  return {UpdateLaunchFailure::StartFailed};
#endif
}

}  // namespace patchy::ui
