#pragma once

#include <QByteArray>
#include <QUrl>
#include <QString>

#include <functional>
#include <optional>

class QObject;

namespace patchy::ui {

struct UpdateInfo {
  QString platform;
  QString version;
  QUrl download_url;
  // The installer Patchy can fetch and run by itself (Windows; docs/auto-update.md). Set
  // only when the manifest entry carries all of installer_url (https), installer_size and
  // installer_sha256; otherwise the update is offered as a browser download.
  QUrl installer_url{};
  qint64 installer_size{0};
  QByteArray installer_sha256{};  // lowercase hex
};

[[nodiscard]] bool update_has_installer(const UpdateInfo& update);

enum class UpdateCheckStatus {
  UpdateAvailable,
  NoUpdateAvailable,
  UnsupportedPlatform,
  InvalidManifest,
  MissingPlatform,
  InvalidVersion,
  InvalidDownloadUrl,
  NetworkError,
};

struct UpdateCheckResult {
  UpdateCheckStatus status{UpdateCheckStatus::InvalidManifest};
  std::optional<UpdateInfo> update;
  QString platform;
  QString latest_version;
  QString detail;
  int http_status{0};
};

using UpdateCheckResultCallback = std::function<void(UpdateCheckResult)>;

// False when something else delivers updates, so Patchy must neither check nor offer
// the setting: the web build (the deployed site is always current), a store build
// (CMake PATCHY_STORE_BUILD, for a repository such as Flathub that forbids apps from
// updating themselves), or PATCHY_NO_UPDATE_CHECK=1 in the environment.
[[nodiscard]] bool update_checks_available();
[[nodiscard]] QString current_update_platform();
[[nodiscard]] QUrl update_manifest_url();
[[nodiscard]] bool update_version_is_newer(const QString& latest_version, const QString& current_version);
[[nodiscard]] UpdateCheckResult inspect_update_manifest(const QByteArray& json, const QString& platform,
                                                        const QString& current_version);
[[nodiscard]] std::optional<UpdateInfo> parse_update_manifest(const QByteArray& json, const QString& platform,
                                                              const QString& current_version);
void request_update_check(QObject* owner, QString current_version, UpdateCheckResultCallback callback);
// One-line user-facing summary of a check result ("Patchy is up to date (0.80).").
[[nodiscard]] QString update_check_status_text(const UpdateCheckResult& result);

}  // namespace patchy::ui
