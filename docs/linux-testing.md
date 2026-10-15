# Linux testing: the Flatpak's own Qt

This page covers the Flatpak. The AppImage is built and tested on its own toolchain
(Ubuntu 22.04, GCC 12, aqt Qt 6.8.3): `packaging/linux/make-appimage.sh --tests` and
`packaging/linux/appimage/test-appimage.sh`, described in [appimage.md](appimage.md).

The Flatpak takes Qt from the
`org.kde.Platform` runtime named in `packaging/linux/flatpak/com.rtsoft.patchy.yml`
(6.11 as of October 2026). The Windows and macOS builds use Qt 6.8.3. Linux verification
therefore runs on the runtime's Qt, never on a different one (Seth, October 13, 2026):
GitHub issue 88 was a Qt 6.11 behavior change that every suite on Qt 6.8.3 passed.

## The rule

- Linux test runs use the Flatpak SDK's Qt. `scripts\remote\remote-build.ps1 -Target
  linux` does this by default.
- A Linux bug report is reproduced on the Flatpak (the installed one, or a build of the
  tree), not on a host build against another Qt.
- A UI fix for a Linux report is verified on that Qt before it is called fixed.
- When the runtime version in the manifest changes, run both full suites again; the
  scripts read the version from the manifest, so nothing else needs editing.

## How it runs

`-Target linux` selects the `linux-flatpak-qt` preset (`build/linux-flatpak-qt`, Release,
tests on, no `CMAKE_PREFIX_PATH`). `scripts/remote/build-and-test.sh` sees that preset,
reads `sdk` and `runtime-version` from the manifest, and re-runs itself inside the SDK:

    flatpak run --user --filesystem=home --command=bash org.kde.Sdk//<version> \
      ~/patchy/src/scripts/remote/build-and-test.sh linux-flatpak-qt [--filter ...]

Inside the sandbox (`/.flatpak-info` exists) the script skips the host tool path and the
dpkg sysroot, so cmake, ninja, the compiler (GCC 15.2.0 for 6.11), Qt and the fonts all
come from the SDK. The source tree and `.deps` (ONNX Runtime, AI models) are the host's,
through `--filesystem=home`. The only prerequisite is the SDK `make-flatpak.sh` already
needs: `flatpak install --user -y flathub org.kde.Platform//<version> org.kde.Sdk//<version>`.

To run one test by hand, enter the same sandbox and run the binary from
`build/linux-flatpak-qt` with `QT_QPA_PLATFORM=offscreen`.

`-NativeQt` selects the older `linux-release` preset (system GCC, the aqt Qt 6.8.3 in
`.deps`). Use it only for tools that need a host binary: the `linux-asan` sync, or a
quick A/B to tell a Qt-version difference from a Patchy bug.

What this does not cover:

- The compiler matches the Flatpak build but the flags do not: flatpak-builder adds
  hardening flags, so a warning from the release build may still need the single-file
  check in [platform.md](platform.md).
- The suites run offscreen. A bug that needs real input or a real window is checked on
  the Flatpak itself in a private X server: `Xvfb :<n>`, then `flatpak run
  --env=QT_QPA_PLATFORM=xcb --env=PATCHY_SETTINGS_DIR=<dir> com.rtsoft.patchy` under
  `dbus-run-session`, or `flatpak-builder --run <build dir> <manifest> patchy` for an
  unreleased tree built with `flatpak-builder <build dir> <manifest>` (no `--repo`, and
  never `make-flatpak.sh`, which deletes the release bundles in `build/package`). XTest
  through Python `ctypes` on `libXtst.so.6` sends real clicks without extra packages;
  ImageMagick `import -window root` captures.
- flatpak-builder sets `SOURCE_DATE_EPOCH` to the manifest's modification time, so the
  in-app build date of such a build is that time, not the build time.

## What differs inside the sandbox

Triage of the first full run (October 13, 2026: 11 UI failures, all passing on the host's
Qt 6.8.3 build) found no Qt 6.11 behavior change among them. The causes were the
sandbox, and one of them was a real bug in the shipped Flatpak:

- **Fonts live elsewhere.** The runtime keeps Liberation and DejaVu in
  `/usr/share/fonts/liberation-fonts` and `/usr/share/fonts/dejavu`, not Debian's
  `/usr/share/fonts/truetype/...`; it ships no CJK face, and the host's fonts appear
  under `/run/host/fonts`. `tests/test_fonts.hpp` lists both layouts. With no registered
  UiDefault font the suite silently runs on a different default face, which is what nine
  of the eleven were (options bar height, clipped labels, "Arial" resolving as a missing
  font).
- **Every run has the same pids.** The sandbox is a pid namespace: the test binary gets
  the same low pid each run, and the Flatpak app is always pid 2. Anything keyed on a
  pid, or trusting a `QLockFile` pid to mean "another live process", is wrong there.
  The suite's per-process stores (`tests/ui/main.cpp`) handle a leftover store that names
  their own pid. In the app this was crash recovery: a crashed instance's lock named the
  scanning instance's own pid, so the Flatpak never offered the crashed documents
  ([document-recovery.md](document-recovery.md)).
- **`FLATPAK_ID` is the SDK's.** The harness unsets it, since the test binary is not the
  installed Flatpak; a test of Flatpak-specific behavior sets it itself.

The eleventh, `ui_tips_psd_speed_mode_line_clip_if_available` (runs only where the local
`tips.psd` fixture exists), was a real text bug that the sandbox's fallback face (DejaVu
Sans, against Noto Sans on the linux build host) made large enough to fail: calibrated
box text grew its render rect on every re-edit. Fixed in the app
([text-render-calibration.md](text-render-calibration.md)).

Source changes the first SDK compile of the tests needed: a missing `<algorithm>`
include, two GCC 15 `-Warray-bounds` false positives in test helpers (fixed by reserving
the vector up front), and `QImage::mirrored`, deprecated from Qt 6.9 (its replacement
`flipped` does not exist in 6.8.3, so tests call a version-checked helper).
