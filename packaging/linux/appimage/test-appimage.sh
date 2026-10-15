#!/usr/bin/env bash
# Runs a built AppImage on clean distros (docs/appimage.md, "Testing"): each distro is a
# fresh podman container with only the libraries a stock desktop install has (no Qt, no
# compiler). Per distro:
#   1. headless script run, including Object Selection (the bundled ONNX Runtime and models)
#   2. real X11 window under Xvfb (xcb plug-in): --screenshot, clean exit
#   3. real Wayland window under a headless weston, where the distro packages weston
#   4. patchy-mcp --check through the AppImage's connector argument
# Containers have no FUSE, so the AppImage extracts itself (APPIMAGE_EXTRACT_AND_RUN): the
# same files a FUSE mount serves. Screenshots land in build/appimage-test/<distro>/.
#   test-appimage.sh <file.AppImage> [ubuntu22 debian12 fedora40 ...]
set -euo pipefail
# Resolved before the cd below, so a path relative to the caller's folder works.
APPIMAGE=$(readlink -f "${1:?usage: test-appimage.sh <file.AppImage> [distro...]}" || true)
[ -n "$APPIMAGE" ] && [ -f "$APPIMAGE" ] || { echo "ERROR: AppImage not found: $1" >&2; exit 1; }
cd "$(dirname "$0")"
ROOT=$(cd ../../.. && pwd)
[ -n "$ROOT" ] && [ -f "$ROOT/CMakeLists.txt" ] || { echo "ERROR: could not find the repository root" >&2; exit 1; }
shift
DISTROS=("$@")
[ ${#DISTROS[@]} -gt 0 ] || DISTROS=(ubuntu22 debian12 fedora40)

# Pinned images and the desktop runtime libraries each needs. These are what a default
# desktop install already has; installing them here only makes the bare image look like one.
DEBIAN_LIBS="libgl1 libegl1 libopengl0 libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 \
  libx11-6 libx11-xcb1 libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
  libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-xfixes0 \
  libxcb-xinerama0 libxcb-xkb1 libxcb-glx0 libxkbcommon0 libxkbcommon-x11-0 libsm6 libice6 \
  libwayland-client0 libwayland-cursor0 libwayland-egl1 libssl3 ca-certificates libcups2 fonts-dejavu-core \
  xvfb xauth weston"
image_for() {
  case "$1" in
    ubuntu22) echo "docker.io/library/ubuntu:22.04@sha256:5ec03bb3441e8b0bf3b4f9cd4629a1ae763010dc3035bb8da3ae6cf026486401" ;;
    debian12) echo "docker.io/library/debian:12@sha256:2c037a04925515fdd6ea85ea14a682d0e79931f5e9f5d07b6dbfc6ba12f9e858" ;;
    fedora40) echo "docker.io/library/fedora:40@sha256:3c86d25fef9d2001712bc3d9b091fc40cf04be4767e48f1aa3b785bf58d300ed" ;;
    *) return 1 ;;
  esac
}
install_for() {
  case "$1" in
    ubuntu22|debian12) echo "apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends $DEBIAN_LIBS >/dev/null" ;;
    fedora40) echo "dnf install -y -q mesa-libGL mesa-libEGL libglvnd-opengl fontconfig freetype dbus-libs glib2 \
      libX11 libX11-xcb libxcb xcb-util-cursor xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil \
      xcb-util libxkbcommon libxkbcommon-x11 libSM libICE libwayland-client libwayland-cursor libwayland-egl \
      openssl-libs ca-certificates cups-libs dejavu-sans-fonts xorg-x11-server-Xvfb xorg-x11-xauth weston which >/dev/null" ;;
  esac
}

OUT="$ROOT/build/appimage-test"
mkdir -p "$OUT"
failed=()
for distro in "${DISTROS[@]}"; do
  image=$(image_for "$distro") || { echo "ERROR: unknown distro $distro" >&2; exit 2; }
  echo "===== $distro ($image) ====="
  rm -rf "${ROOT:?}/build/appimage-test/$distro"
  mkdir -p "$OUT/$distro"
  if podman run --rm --security-opt label=disable \
      --volume "$APPIMAGE:/test/Patchy.AppImage:ro" --volume "$OUT/$distro:/out" \
      --volume "$ROOT/packaging/linux/appimage/test-in-container.sh:/test/run.sh:ro" \
      --env APPIMAGE_EXTRACT_AND_RUN=1 --env PATCHY_NO_SOUND=1 \
      "$image" bash -c "$(install_for "$distro") && bash /test/run.sh"; then
    echo "===== $distro: PASS ====="
  else
    echo "===== $distro: FAIL =====" >&2
    failed+=("$distro")
  fi
done
if [ ${#failed[@]} -gt 0 ]; then
  echo "FAILED: ${failed[*]}" >&2
  exit 1
fi
echo "All distros passed: ${DISTROS[*]} (screenshots in $OUT)"
