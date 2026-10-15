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
  // installer_sha256, and its optional installer_min_version does not exclude the running
  // version; otherwise the update is offered as a browser download.
  QUrl installer_url{};
  qint64 installer_size{0};
  QByteArray installer_sha256{};  // lowercase hex
  // Linux only: where an AppImage copy downloads the new release (the permanent manifest
  // key appimage_url in the linux entry; http(s)). Empty when the manifest has none, and
  // the AppImage update prompt then falls back to kAppImageDownloadUrl.
  QUrl appimage_url{};
  // The AppImage an AppImage copy can install by itself (docs/auto-update.md): set only when
  // the linux entry carries all of appimage_install_url (an https GitHub release download of
  // this repository, kAppImageReleaseDownloadPrefix), appimage_install_size and
  // appimage_install_sha256, and its optional appimage_min_version does not exclude the
  // running version.
  QUrl appimage_install_url{};
  qint64 appimage_install_size{0};
  QByteArray appimage_install_sha256{};  // lowercase hex
};

// The GitHub latest-release permalink of the AppImage (a stable asset name, like
// PatchyLinux.flatpak; docs/release-process.md).
inline constexpr char kAppImageDownloadUrl[] =
    "https://github.com/SethRobinson/Patchy/releases/latest/download/PatchyLinux.AppImage";
// An AppImage has no publisher signature to check (the Windows installer's second guard),
// so the file it installs must also come from this repository's GitHub releases: a manifest
// edit alone cannot point it at another host.
inline constexpr char kAppImageReleaseDownloadPrefix[] = "https://github.com/SethRobinson/Patchy/releases/download/";

[[nodiscard]] bool update_has_appimage_install(const UpdateInfo& update);

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
// The interval gate for the startup check (GitHub issue 92): true when `interval_days` is 0
// (every launch), no check is recorded, the record lies in the future (the clock moved
// back), or that many days have passed. Times are UTC seconds since the epoch.
[[nodiscard]] bool startup_update_check_due(int interval_days, qint64 last_check_secs, qint64 now_secs);
// Whether the startup check should raise the update prompt for `version`: not when the user
// ticked "Don't ask again" for it (`skipped_version`) or for a later one. A stored value
// that is empty or not a version never hides a prompt.
[[nodiscard]] bool update_prompt_wanted(const QString& version, const QString& skipped_version);
void request_update_check(QObject* owner, QString current_version, UpdateCheckResultCallback callback);
// One-line user-facing summary of a check result ("Patchy is up to date (0.80).").
[[nodiscard]] QString update_check_status_text(const UpdateCheckResult& result);

}  // namespace patchy::ui
