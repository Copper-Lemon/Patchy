# Linux testing: the Flatpak's own Qt

Patchy ships on Linux only as a Flatpak, and the Flatpak takes Qt from the
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

## Known failures on Qt 6.11 (untriaged, October 13, 2026)

The first full run inside the SDK: core suite 1207 pass, 0 fail; UI suite 1748 pass, 11
fail. All 11 pass on the host's Qt 6.8.3 build. Each is either a real difference Flatpak
users see or a font/environment assumption in the test, and none has been investigated
yet. Remove an entry when it is fixed or explained.

- `ui_brush_tip_picker_keeps_options_bar_height` (options bar height)
- `ui_options_bar_transform_session_replaces_tool_controls` (options bar height)
- `ui_animation_preview_fits_its_text_at_larger_fonts` (clipped text)
- `ui_text_reedit_preserves_rich_text_spacing`
- `ui_text_editor_ctrl_b_and_ctrl_i_toggle_formatting` (font picker face is not "Bold Italic")
- `ui_tips_psd_speed_mode_line_clip_if_available`
- `ui_text_layer_font_without_glyph_coverage_counts_as_missing`
- `ui_image_size_dialog_scales_box_text_frame_and_size`
- `ui_typing_uncovered_characters_switches_their_font`
- `ui_script_text_setters_warn_about_fonts`
- `ui_ai_setup_dialog_opens_from_help` (the open-skill button is disabled)

Source changes the first SDK compile of the tests needed: a missing `<algorithm>`
include, two GCC 15 `-Warray-bounds` false positives in test helpers (fixed by reserving
the vector up front), and `QImage::mirrored`, deprecated from Qt 6.9 (its replacement
`flipped` does not exist in 6.8.3, so tests call a version-checked helper).
