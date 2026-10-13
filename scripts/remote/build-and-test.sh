#!/usr/bin/env bash
# Remote half of scripts/remote/remote-build.ps1. Runs inside ~/patchy/src right after
# the snapshot checkout: configure + build the given preset, then run both test suites.
#   build-and-test.sh <preset> [--filter <substr>] [--skip-tests]
set -euo pipefail

PRESET="${1:?usage: build-and-test.sh <preset> [--filter substr] [--skip-tests]}"
shift

# linux-flatpak-qt builds and tests against the Qt Linux users actually run: the
# org.kde.Sdk the Flatpak manifest names (compiler, Qt and fonts all come from it, never
# from .deps or the host). Re-run this script inside that sandbox; /.flatpak-info marks
# being inside. The SDK is the one make-flatpak.sh already needs installed.
if [ "$PRESET" = "linux-flatpak-qt" ] && [ ! -f /.flatpak-info ]; then
  MANIFEST="$HOME/patchy/src/packaging/linux/flatpak/com.rtsoft.patchy.yml"
  SDK=$(sed -nE 's/^sdk:[[:space:]]*([A-Za-z0-9._-]+).*$/\1/p' "$MANIFEST" | head -1)
  SDK_VERSION=$(sed -nE "s/^runtime-version:[[:space:]]*'?([0-9.]+)'?.*$/\1/p" "$MANIFEST" | head -1)
  if [ -z "$SDK" ] || [ -z "$SDK_VERSION" ]; then
    echo "ERROR: could not read sdk and runtime-version from $MANIFEST" >&2
    exit 1
  fi
  echo "== entering $SDK//$SDK_VERSION =="
  exec flatpak run --user --filesystem=home --command=bash "$SDK//$SDK_VERSION" \
    "$HOME/patchy/src/scripts/remote/build-and-test.sh" "$PRESET" "$@"
fi

FILTER=""
SKIP_TESTS=0
while [ $# -gt 0 ]; do
  case "$1" in
    --filter) FILTER="${2:?--filter needs a value}"; shift 2 ;;
    --skip-tests) SKIP_TESTS=1; shift ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

# Host tools and the sudo-free dependency sysroot (see setup-linux.sh: dev packages
# extracted with dpkg-deb -x when apt install wasn't possible) are for native builds
# only. Inside the Flatpak SDK they would shadow its cmake, ninja and libraries.
if [ ! -f /.flatpak-info ]; then
  export PATH="$HOME/.patchy-tools/bin:$HOME/.local/bin:$PATH"
fi
if [ ! -f /.flatpak-info ] && [ -d "$HOME/.patchy-tools/sysroot/usr" ]; then
  export CMAKE_PREFIX_PATH="$HOME/.patchy-tools/sysroot/usr${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
  export LD_LIBRARY_PATH="$HOME/.patchy-tools/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

cd "$HOME/patchy/src"
# Every core but four (at least one), so the remote machine stays usable.
JOBS=$(( $(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 5) - 4 ))
[ "$JOBS" -ge 1 ] || JOBS=1

echo "== configure ($PRESET) =="
cmake --preset "$PRESET"
echo "== build ($PRESET) =="
nice -n 10 cmake --build --preset "$PRESET" -j "$JOBS"

if [ "$SKIP_TESTS" = "1" ]; then
  echo "== tests skipped =="
  exit 0
fi

BUILD_DIR="build/$PRESET"

echo "== patchy_core_tests =="
(cd "$BUILD_DIR" && nice -n 10 ./patchy_core_tests)

echo "== patchy_ui_visual_tests (offscreen) =="
if [ -n "$FILTER" ]; then
  (cd "$BUILD_DIR" && QT_QPA_PLATFORM=offscreen nice -n 10 ./patchy_ui_visual_tests "$FILTER")
else
  (cd "$BUILD_DIR" && QT_QPA_PLATFORM=offscreen nice -n 10 ./patchy_ui_visual_tests)
fi
