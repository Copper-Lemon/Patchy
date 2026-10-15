#!/usr/bin/env bash
# Measures where a Wayland compositor puts Qt popups at an interface scale (docs/wayland.md,
# GitHub issue 100). Runs on a Linux host with podman and the Flatpak SDK, never on a desktop
# session: a headless sway in a Debian 12 container serves its own socket, the probe and a
# virtual pointer build and run inside the manifest's org.kde.Sdk (the Flatpak's Qt), the
# pointer clicks the menu bar item or the combo box, and the output lists the anchors Qt
# sent (xdg_positioner) and where the compositor put each popup (xdg_popup.configure,
# relative to the parent surface). Screenshots land in build/wayland-popups/.
#   run.sh <scale> [menu|combo] [fix]      e.g. run.sh 1.25 menu fix
# KEEP=1 leaves the sway container running for the next call.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
[ -n "$ROOT" ] && [ -f "$ROOT/CMakeLists.txt" ] || { echo "ERROR: could not find the repository root" >&2; exit 1; }
scale=${1:?usage: run.sh <scale> [menu|combo] [fix]}
target=${2:-menu}
fix=${3:-}
OUT="$ROOT/build/wayland-popups"
mkdir -p "$OUT"
MANIFEST="$ROOT/packaging/linux/flatpak/com.rtsoft.patchy.yml"
SDK="org.kde.Sdk//$(sed -nE "s/^runtime-version:[[:space:]]*'?([0-9.]+)'?.*$/\1/p" "$MANIFEST" | head -1)"
IMAGE="docker.io/library/debian:12@sha256:2c037a04925515fdd6ea85ea14a682d0e79931f5e9f5d07b6dbfc6ba12f9e858"
CONTAINER=patchy-wayland-popups
RUNTIME_DIR=${XDG_RUNTIME_DIR:?XDG_RUNTIME_DIR must be set}
in_sdk() { flatpak run --user --filesystem=home --socket=wayland --command=bash "$SDK" -c "$1"; }

# Build the probe and the virtual pointer with the SDK's toolchain and Qt.
[ -s "$OUT/vptr.xml" ] || curl -sfL -o "$OUT/vptr.xml" \
  https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/raw/master/unstable/wlr-virtual-pointer-unstable-v1.xml
in_sdk "set -e; cmake -S '$HERE' -B '$OUT/build' -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build '$OUT/build' >/dev/null
  cd '$OUT' && wayland-scanner client-header vptr.xml vptr-client.h && wayland-scanner private-code vptr.xml vptr-protocol.c
  gcc -O1 -I'$OUT' -o '$OUT/vptr' '$HERE/vptr.c' vptr-protocol.c \$(pkg-config --cflags --libs wayland-client)"

# Headless sway as this user (keep-id), so its sockets in the runtime dir are ours. Its
# config reports the socket names, which wlroots picks itself.
if ! podman container exists "$CONTAINER" || [ "$(podman inspect -f '{{.State.Running}}' "$CONTAINER")" != true ]; then
  podman rm -f "$CONTAINER" >/dev/null 2>&1 || true
  rm -f "$OUT/display" "$OUT/swaysock"
  cat > "$OUT/sway.conf" <<'EOF'
output HEADLESS-1 resolution 1400x900 position 0 0
default_border none
focus_follows_mouse no
exec echo $WAYLAND_DISPLAY > /out/display
exec echo $SWAYSOCK > /out/swaysock
EOF
  podman run -d --name "$CONTAINER" --userns=keep-id --user 0 --security-opt label=disable \
    --volume "$RUNTIME_DIR:/xdg" --volume "$OUT:/out" "$IMAGE" bash -c "
      apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends sway grim >/dev/null &&
      XDG_RUNTIME_DIR=/xdg WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman WAYLAND_DISPLAY= \
        exec setpriv --reuid=$(id -u) --regid=$(id -g) --clear-groups sway --unsupported-gpu -c /out/sway.conf" >/dev/null
  for _ in $(seq 180); do [ -s "$OUT/display" ] && break; sleep 1; done
  [ -s "$OUT/display" ] || { echo "ERROR: sway did not start" >&2; podman logs "$CONTAINER" | tail -20 >&2; exit 1; }
fi
WD=$(cat "$OUT/display")
SWAYSOCK_IN=$(cat "$OUT/swaysock")

tag="$scale-$target${fix:+-fix}"
log="$OUT/$tag.log"
: > "$log"
(
  # Click the target the probe reports, in surface pixels.
  for _ in $(seq 50); do grep -q '^click targets' "$log" && break; sleep 0.1; done
  read -r ix iy iw ih cx cy cw ch < <(sed -nE 's/^click targets.*Image item \(([0-9-]+),([0-9-]+) ([0-9]+)x([0-9]+)\), combo \(([0-9-]+),([0-9-]+) ([0-9]+)x([0-9]+)\)/\1 \2 \3 \4 \5 \6 \7 \8/p' "$log")
  if [ "$target" = combo ]; then x=$((cx + cw / 2)); y=$((cy + ch / 2)); else x=$((ix + iw / 2)); y=$((iy + ih / 2)); fi
  read -r nx ny < <(awk "BEGIN{printf \"%d %d\n\", $x*$scale, $y*$scale}")
  printf 'move %d %d\nsleep 300\nclick\nsleep 5500\n' "$nx" "$ny" |
    WAYLAND_DISPLAY=$WD flatpak run --user --filesystem=home --socket=wayland --command="$OUT/vptr" "$SDK" 1400 900 &
  sleep 3.6
  podman exec --user "$(id -u)" -e XDG_RUNTIME_DIR=/xdg -e WAYLAND_DISPLAY="$WD" -e SWAYSOCK="$SWAYSOCK_IN" \
    "$CONTAINER" grim "/out/$tag.png"
  wait
) &
WAYLAND_DISPLAY=$WD flatpak run --user --filesystem=home --socket=wayland --env=WAYLAND_DEBUG=1 \
  --env=QT_QPA_PLATFORM=wayland --env=QT_SCALE_FACTOR="$scale" ${fix:+--env=PROBE_FIX=1} \
  --command="$OUT/build/popup_probe" "$SDK" >> "$log" 2> "$OUT/$tag.wayland.txt"
wait
cat "$log"
grep -E 'positioner[@#][0-9]+\.set_(anchor_rect|size)|xdg_popup[@#][0-9]+\.configure' "$OUT/$tag.wayland.txt" |
  sed -E 's/^\[[^]]*\] *//; s/\{[^}]*\} *//'
echo "screenshot: $OUT/$tag.png"
if [ -z "${KEEP:-}" ]; then
  podman rm -f "$CONTAINER" >/dev/null
  # sway dies with its container and leaves its sockets behind.
  rm -f "${RUNTIME_DIR:?}/$WD" "${RUNTIME_DIR:?}/$WD.lock" "${RUNTIME_DIR:?}/$(basename "$SWAYSOCK_IN")" "$OUT/display" "$OUT/swaysock"
fi
