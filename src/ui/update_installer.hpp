#pragma once

// One-click update on Windows (docs/auto-update.md): the update prompt downloads the
// installer the manifest pins by size and SHA-256, closes Patchy through the normal save
// prompts, and hands over to the installer's update mode, which waits for this process,
// installs into the same folder and starts Patchy again. Only a copy the installer made can
// do this; the portable zip, a development build and every other platform keep the browser
// download.

#include <QString>
#include <QtGlobal>

namespace patchy::ui {

struct UpdateInstallerLaunch {
  QString installer_path;
  QString install_dir;         // where this copy is installed; the installer updates it in place
  qint64 wait_process_id{0};   // the installer waits for this process to exit first
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
// <temp>/patchy-update-<version>, where the installer for `version` is downloaded.
[[nodiscard]] QString update_download_folder(const QString& version);
// Deletes the installer downloaded for `version` (by its exact name) and its then-empty
// folder. Run by the updated copy once the installer has exited; missing files are fine.
void remove_update_download(const QString& version);

// Starts the installer detached in update mode. The request travels in the child's
// environment (PATCHY_UPDATE_INSTALL_DIR, PATCHY_UPDATE_WAIT_PID), which
// packaging/windows/InstallPatchyLauncher.cs turns into InstallPatchy.ps1 -Update.
// False when it could not be started (always off Windows).
bool launch_update_installer(const UpdateInstallerLaunch& launch);

}  // namespace patchy::ui
