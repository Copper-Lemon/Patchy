#!/usr/bin/env bash
# Runs the installed Patchy Flatpak on the headless sway rig that run.sh left behind (KEEP=1)
# and captures it (docs/wayland.md). Pointer input goes through the vptr client.
#   run-patchy.sh start <scale> [extra flatpak args...]   launches patchy (single instance), waits, screenshots
#   run-patchy.sh script <file.js> <tag>                 forwards --run-script to the running instance, screenshots
#   run-patchy.sh click <x> <y> <tag>                    clicks at native pixels, screenshots after 1.5 s
#   run-patchy.sh drag <x1> <y1> <x2> <y2> <tag>         press, move in steps, release, screenshot
#   run-patchy.sh shot <tag>
#   run-patchy.sh stop                                   flatpak kill com.rtsoft.patchy
set -u
export XDG_RUNTIME_DIR=/run/user/$(id -u)
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
[ -n "$ROOT" ] && [ -f "$ROOT/CMakeLists.txt" ] || { echo "ERROR: could not find the repository root" >&2; exit 1; }
OUT=$ROOT/build/wayland-108
POP=$ROOT/build/wayland-popups
mkdir -p "$OUT"
WD=$(cat "$POP/display"); SOCK=$(cat "$POP/swaysock")
SDK="org.kde.Sdk//6.11"
CONTAINER=patchy-wayland-popups
SCALE_FILE=$OUT/scale
shot() { podman exec --user "$(id -u)" -e XDG_RUNTIME_DIR=/xdg -e WAYLAND_DISPLAY="$WD" -e SWAYSOCK="$SOCK" "$CONTAINER" grim "/out/$1.png" && cp "$POP/$1.png" "$OUT/$1.png" && echo "shot: $OUT/$1.png"; }
vptr() { WAYLAND_DISPLAY=$WD flatpak run --user --filesystem=home --socket=wayland --command="$POP/vptr" "$SDK" 1400 900; }
run_patchy() { WAYLAND_DISPLAY=$WD flatpak run --user --socket=wayland --filesystem=home --env=QT_QPA_PLATFORM=wayland --env=QT_SCALE_FACTOR="$(cat "$SCALE_FILE")" --env=PATCHY_SETTINGS_DIR="$OUT/settings" "$@"; }
case "$1" in
  start)
    echo "$2" > "$SCALE_FILE"; shift 2
    flatpak kill com.rtsoft.patchy 2>/dev/null; sleep 1
    rm -rf "$OUT/settings"; mkdir -p "$OUT/settings"
    nohup bash -c "$(declare -f run_patchy); WD=$WD; SCALE_FILE=$SCALE_FILE; OUT=$OUT; run_patchy $* com.rtsoft.patchy" > "$OUT/patchy.log" 2>&1 &
    sleep 12
    shot "start-$(cat "$SCALE_FILE")"
    ;;
  script)
    cp "$2" "$OUT/forward.js"; rm -f "$OUT/forward.out"
    timeout 60 bash -c "$(declare -f run_patchy); WD=$WD; SCALE_FILE=$SCALE_FILE; OUT=$OUT; run_patchy com.rtsoft.patchy --run-script '$OUT/forward.js' --script-output '$OUT/forward.out'"
    sleep 2; cat "$OUT/forward.out" 2>/dev/null
    shot "$3"
    ;;
  click)
    printf 'move %d %d\nsleep 200\nclick\nsleep 1500\n' "$2" "$3" | vptr
    sleep 0.5; shot "$4"
    ;;
  drag)
    { printf 'move %d %d\nsleep 200\npress\nsleep 100\n' "$2" "$3"
      for i in 1 2 3 4 5 6 7 8; do printf 'move %d %d\nsleep 60\n' $(( $2 + ($4 - $2) * i / 8 )) $(( $3 + ($5 - $3) * i / 8 )); done
      printf 'release\nsleep 1500\n'; } | vptr
    sleep 0.5; shot "$6"
    ;;
  shot) shot "$2" ;;
  stop) flatpak kill com.rtsoft.patchy; sleep 1; pgrep -af "com.rtsoft.patchy|/app/bin/patchy" | grep -v pgrep | head ;;
esac
