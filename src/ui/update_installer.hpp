#pragma once

// One-click update on Windows (docs/auto-update.md): the update prompt downloads the
// installer the manifest pins by size and SHA-256, closes Patchy through the normal save
// prompts, and hands over to the installer's update mode, which waits for this process,
// installs into the same folder and starts Patchy again. Only a copy the installer made can
// do this; the portable zip, a development build and every other platform keep the browser
// download.

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QtGlobal>

namespace patchy::ui {

struct UpdateInstallerLaunch {
  QString installer_path;
  QString install_dir;         // where this copy is installed; the installer updates it in place
  qint64 wait_process_id{0};   // the installer waits for this process to exit first
  // What the manifest pinned. The file is checked against these again at the moment it is
  // started, not only when it was downloaded.
  qint64 expected_size{0};
  QByteArray expected_sha256{};  // lowercase hex
  // Where the browser download is, offered when the installer cannot be run.
  QUrl browser_download_url{};
  // An AppImage update (docs/auto-update.md, "AppImage"): installer_path is the downloaded
  // AppImage, and instead of running it, launch_update_installer renames it over this file
  // (the running .AppImage) and starts it once wait_process_id has exited.
  QString appimage_target{};
};

// Why an installer was not started; the prompt tells the user which.
enum class UpdateLaunchFailure {
  None,
  InstallerMissing,   // the downloaded file is gone or cannot be opened
  ChecksumMismatch,   // not the size and SHA-256 the manifest pinned
  NotSigned,          // no valid Authenticode signature, and this Patchy is signed
  WrongSigner,        // validly signed, by someone other than this Patchy's publisher
  StartFailed,        // Windows would not start it
  ReplaceFailed,      // AppImage: the downloaded file could not be put in place of the running one
};
struct UpdateLaunchResult {
  UpdateLaunchFailure failure{UpdateLaunchFailure::None};
  QString expected_signer{};   // NotSigned, WrongSigner: this Patchy's publisher
  QString installer_signer{};  // WrongSigner: who signed the file
  [[nodiscard]] bool started() const { return failure == UpdateLaunchFailure::None; }
};

// True when `registered_install_location` (the uninstall entry's InstallLocation) names
// `application_dir`. Separators, a trailing separator and letter case do not matter.
[[nodiscard]] bool install_location_matches(const QString& registered_install_location,
                                            const QString& application_dir);
// True when this running copy was put there by the Windows installer: the per-user
// uninstall entry names the application folder and UninstallPatchy.exe sits beside
// patchy.exe. Always false off Windows.
[[nodiscard]] bool installed_copy_can_self_update();

[[nodiscard]] QString update_installer_file_name();
// <per-user app data>/updates/patchy-update-<version>, where the installer for `version` is
// downloaded. Never the shared temp folder: another account must not be able to put a file
// where Patchy will run one.
[[nodiscard]] QString update_download_folder(const QString& version);
// Deletes the installer downloaded for `version` (by its exact name) and its then-empty
// folders. Run by the updated copy once the installer has exited; missing files are fine.
void remove_update_download(const QString& version);

// True when the file at `path` has exactly this size and SHA-256.
[[nodiscard]] bool file_matches_checksum(const QString& path, qint64 size, const QByteArray& sha256_hex);

// A file's embedded Authenticode signature: `valid` when Windows verifies it up to a trusted
// root (no revocation lookup, no network), `signer` the signing certificate's display name.
// Never valid off Windows.
struct FileSignature {
  bool valid{false};
  QString signer{};
};
[[nodiscard]] FileSignature file_signature(const QString& path);
// A signed Patchy only runs an installer signed by the same publisher, so a manifest that
// names some other file (a bad push, a taken-over account) cannot run code by its checksum
// alone. An unsigned Patchy (a development or trial build) has no publisher to compare with
// and accepts the checksum.
[[nodiscard]] bool installer_signer_acceptable(const FileSignature& running_app, const FileSignature& installer);

// AppImage self-update (docs/auto-update.md, "AppImage"). The new AppImage downloads beside
// the running one, as a hidden file with this name, so the final rename stays on one file
// system: ".<file name>.update-<version>".
[[nodiscard]] QString appimage_update_download_name(const QString& appimage_path, const QString& version);
// True when the running .AppImage can be replaced: it is a file and its folder is writable
// (a rename needs the folder, not the file). An AppImage in a read-only place keeps the
// browser download.
[[nodiscard]] bool appimage_can_be_replaced(const QString& appimage_path);
// Checks the downloaded AppImage once more (size and SHA-256 as pinned), makes it executable
// and renames it over launch.appimage_target. The running copy keeps its mount, so it carries
// on until it exits. Returns InstallerMissing, ChecksumMismatch or ReplaceFailed, or success.
[[nodiscard]] UpdateLaunchResult replace_appimage(const UpdateInstallerLaunch& launch);
// Deletes the AppImage downloaded for `version` beside `appimage_path` (by its exact name),
// left behind when an update was cancelled at the save prompt. Missing files are fine.
void remove_appimage_update_download(const QString& appimage_path, const QString& version);

// Starts the installer detached in update mode, after checking it once more: size and
// SHA-256 as pinned, and the signer rule above, with the file held open against writes from
// the check until the process exists. The request travels in the child's environment
// (PATCHY_UPDATE_INSTALL_DIR, PATCHY_UPDATE_WAIT_PID), which
// packaging/windows/InstallPatchyLauncher.cs turns into InstallPatchy.ps1 -Update.
// The result names the check that failed, or StartFailed (always, off Windows).
// With appimage_target set it runs replace_appimage instead and, on Linux, starts a /bin/sh
// waiter that launches the new AppImage once wait_process_id has exited (at most two
// minutes later). A waiter that cannot start does not fail the update: the file is already
// replaced, and the next launch runs the new version.
[[nodiscard]] UpdateLaunchResult launch_update_installer(const UpdateInstallerLaunch& launch);

}  // namespace patchy::ui
