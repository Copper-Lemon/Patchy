# One-click update (Windows)

Read this before touching the update prompt, `latest_version.json`'s installer keys, the
installer's update mode, or the release announcement step.

On Windows, a copy of Patchy that the installer put there can update itself from the update
prompt: **Update Now** downloads the installer, closes Patchy through the normal save
prompts, installs into the same folder, and starts Patchy again. Everything else keeps the
browser download: the portable zip, a development build, macOS, Linux, and any manifest that
does not carry the installer keys.

## Flow

1. `request_update_check` (`src/ui/update_checker.cpp`) reads the manifest. The `windows`
   entry may carry three extra keys, all or none:
   `installer_url` (https, the versioned `releases/download/v<version>/...` URL, so the file
   behind it never changes), `installer_size` (bytes) and `installer_sha256` (hex).
   `update_has_installer()` is true only with all three valid. The key names are permanent.
   Builds older than this feature ignore them and use `download_url`.
2. `MainWindow::can_install_update` also needs `installed_copy_can_self_update()`
   (`src/ui/update_installer.cpp`): the per-user uninstall entry
   `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Patchy` names the folder the
   app runs from as `InstallLocation`, and `UninstallPatchy.exe` sits beside `patchy.exe`.
3. `MainWindow::download_and_install_update` (`src/ui/main_window_files.cpp`) downloads
   into `<per-user app data>/updates/patchy-update-<version>/PatchyWindowsInstaller.exe` (never the
   shared temp folder) with
   `start_model_download`, the same downloader as the Object Selection model: every host
   resolves on a detached thread first, redirects must be https, the file is written through
   `QSaveFile`, and it is kept only when size and SHA-256 match the manifest. A file already
   there with the right hash is reused. A failure offers the browser download instead.
4. It then sets `pending_update_install_` and calls `close()`. `closeEvent` asks about
   unsaved documents as on any quit. Cancel there drops the pending update and Patchy stays
   open. Once every document is settled, `launch_update_installer` starts the installer
   detached with `PATCHY_UPDATE_INSTALL_DIR` and `PATCHY_UPDATE_WAIT_PID` in its
   environment. It first checks the file again (the save prompts may have taken minutes): size
   and SHA-256 as pinned, and the signer rule below, holding the file open against writes
   until the process exists. If a check fails or it cannot start, the close is refused so
   Patchy does not quit un-updated.
5. `packaging/windows/InstallPatchyLauncher.cs` turns those variables into
   `InstallPatchy.ps1 -Update -InstallDir <dir> -WaitForProcessId <pid>` and removes them
   from the child's environment. Without the variable the wizard runs as before. The
   self-extractor takes no arguments in either case.
6. `InstallPatchy.ps1 -Update` waits up to two minutes for the process to exit (Patchy can
   take about ten seconds to finish background work), shows a small "Updating Patchy to
   version N..." window, installs with the wizard's own install function (in a second copy
   of the script, `-UpdateWorker`, so the window keeps animating instead of going "Not
   Responding"), keeps the desktop shortcut only
   if there is one, and starts the installed `patchy.exe`. A problem (Patchy still running,
   a file in use, a failed copy) shows the same Retry or error box the wizard would.
7. If the script fails or never runs, the launcher starts the installed `patchy.exe` again
   when it is still there, or says where to download the installer.
8. The updated copy deletes the downloaded installer 30 seconds after startup
   (`remove_update_download`, by exact file name, then the empty folder).

## Switching it off from the manifest

If a release's self-update turns out to be broken, edit `latest_version.json` on `main`; no
client has to change. Both switches leave the update prompt itself working, with the old
"download in the browser" button.

- **Only for versions older than X:** add `"installer_min_version": "X"` to the `windows`
  entry. Copies running a version older than X keep the browser download; X and newer still
  install by themselves. Example: 1.10's updater is broken and 1.11 fixes it, so the entry
  for 1.11 carries `"installer_min_version": "1.11"`. A value that is not a version string
  switches self-install off for everyone, so a typo fails safe.
- **For everyone:** remove the three installer keys (or just one of them).

The key name is permanent, like the other three.

## Rules

- Two independent checks guard the installer. Its size and SHA-256 come from the manifest
  on `main`, fetched over https. And a signed Patchy runs it only when Windows verifies its
  Authenticode signature and the signer is the publisher that signed the running `patchy.exe`
  (`file_signature`, `installer_signer_acceptable`): the download carries no Mark of the Web
  and shows no SmartScreen prompt, so without this a bad push to the manifest alone could run
  any file. An unsigned Patchy (development, the trial script) has no publisher and relies on
  the checksum. Never accept an installer whose size or hash was not in the manifest, never
  run one from `download_url`, and never drop the signer check to make an update work.
- Nothing from the manifest except the version reaches an AI assistant as text.
- Nothing is ever killed. The installer waits for the Patchy that asked for the update;
  AI apps are left alone (see below).
- The install is per-user and needs no elevation, so there is no UAC prompt in the flow.
- Announcing a release now means adding the three keys to the `windows` entry as well as
  bumping `version`; `publish-github-release.ps1` prints them. Still announce last
  (docs/release-process.md). A manifest with a new `version` and stale installer keys would
  fail the checksum and fall back to the browser download.

## Connectors that are running during an update

An AI app keeps one `patchy-mcp.exe` per session, often for days, and each is a whole
Patchy holding about twenty files open (the executable, the Qt DLLs, plug-ins, ONNX
Runtime). Stopping them would destroy an agent's unsaved isolated workspace, so the update
works around them:

- **The installer sets locked files aside.** Before removing anything, every install mode
  runs `Confirm-PatchyFilesFreeForInstall`: each installed file another program holds open
  is renamed to `<name>.old-<yyyyMMddHHmmss>` (Windows lets a loaded image be renamed, not
  replaced), and the new copy takes its name. The old process keeps running the old code.
  Only a file that cannot even be renamed stops the install with a Retry box naming it.
  This happens after the payload is unpacked and checked, immediately before the old files
  are removed, and a replacement that then fails gives each set-aside file its name back
  (`Restore-PatchySetAsideFiles`). The check also covers the new version's file list, so an
  install with a missing or damaged manifest is handled the same way.
  The suffix goes after the extension so Qt never loads a set-aside plug-in DLL.
- **Set-aside files stay in `PatchyInstallManifest.txt`**, so the next install and the
  uninstaller delete them once nothing uses them; one still in use is carried forward.
- **Patchy tells the user after the restart.** Update mode lists the apps whose connector
  ran from the install (the connector's parent process, by product name) and starts Patchy
  with them in `PATCHY_UPDATE_CONNECTOR_CLIENTS` (`|` separated).
  `MainWindow::show_connector_restart_notice` shows them once: restart these apps to finish
  updating. No user-facing text says "reconnect": most AI apps have no such control.
- **The old connector tells the assistant.** `ui/connector_freshness.*` remembers the
  executable's size and modification time at startup; once a different file sits at that
  path, every tool result from an owned connector or the attached proxy carries
  `updateNotice`. New sessions start the new connector by themselves.
- **A connector also checks for updates**, for people who never open the app: an owned
  connector runs `request_update_check` once (same switches as the app, the user's
  `updates/checkOnStartup` setting read from their real settings file, never for a suite
  with its own `PATCHY_SETTINGS_DIR`), and `get_info` reports a newer version as
  `updateAvailable`. The attached proxy does not: its Patchy shows the prompt itself.

What an old session can still get wrong until its app restarts: help text and bundled
scripts are read from disk per request, so they are the new version's, and a plug-in it
had not loaded yet would be the new build.

## Tests

- `tests/ui/app_shell_tests.cpp`, filter `update`: the manifest keys, the install-location
  match, the prompt for an installed copy, and the whole path with a `file://` stand-in
  installer and the launch captured through `MainWindowTestAccess` (wrong checksum, Cancel
  at the save prompt, a launch that fails, a settled close).
  `ui_update_connector_restart_notice_names_the_apps` and
  `connector_update_notice_reports_a_replaced_executable` cover the connector notices.
- `scripts\release\verify-windows-package.ps1` runs `InstallPatchy.ps1 -UpdateCheckRoot`:
  the process wait, the locked-file check, and setting a file in use aside (renamed,
  recorded in the manifest, removed by the next install) on stand-in files in a scratch folder.
- Not automated: the real install and relaunch. It writes the uninstall entry and
  shortcuts, so it cannot run on a machine whose installed Patchy must stay untouched.
  Before a release that changes any of this, install an older package into a scratch
  folder, point `PATCHY_UPDATE_MANIFEST_URL` at a manifest naming the new installer, and
  click through it with an unsaved document open. With that variable set, `installer_url`
  may be http, so a loopback server can serve both files.
  `scripts\dev\auto-update-trial.ps1` builds such a trial from `build\release` without
  touching `build\package`.
