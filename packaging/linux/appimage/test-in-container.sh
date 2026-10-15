#!/usr/bin/env bash
# Inside one clean-distro container of test-appimage.sh: /test/Patchy.AppImage is the
# AppImage under test, /out collects logs and screenshots.
set -uo pipefail
APP=/test/Patchy.AppImage
export PATCHY_SETTINGS_DIR=/tmp/patchy-settings
mkdir -p "$PATCHY_SETTINGS_DIR"
status=0
fail() { echo "FAIL: $*" >&2; status=1; }

echo "-- glibc: $(ldd --version | head -1)"

echo "-- 1. headless script with Object Selection"
cat > /tmp/smoke.js <<'EOF'
const doc = app.newDocument(256, 256);
if (!doc.selection.automaticSelectionAvailable) throw new Error("automatic selection is unavailable");
doc.selection.objectSelect({rect: {x: 32, y: 32, width: 128, height: 128}});
console.log("object selection ran");
EOF
if timeout 300 "$APP" --headless --run-script /tmp/smoke.js --script-output /out/headless.txt > /out/headless-console.txt 2>&1 \
   && [ "$(tail -n 1 /out/headless.txt)" = "[done]" ] && grep -q "object selection ran" /out/headless.txt; then
  echo "ok"
else
  fail "headless script"; cat /out/headless-console.txt /out/headless.txt 2>/dev/null
fi

echo "-- 2. X11 window (xcb) under Xvfb"
Xvfb :42 -screen 0 1600x1000x24 -nolisten tcp > /out/xvfb.log 2>&1 &
xvfb=$!
sleep 2
if DISPLAY=:42 QT_QPA_PLATFORM=xcb timeout 180 "$APP" --screenshot /out/xcb.png > /out/xcb-console.txt 2>&1 \
   && [ -s /out/xcb.png ]; then
  echo "ok"
else
  fail "xcb window (exit or crash at quit)"; cat /out/xcb-console.txt
fi
kill "$xvfb" 2>/dev/null

echo "-- 3. Wayland window under headless weston"
if command -v weston >/dev/null; then
  export XDG_RUNTIME_DIR=/tmp/xdg-runtime
  mkdir -p -m 700 "$XDG_RUNTIME_DIR"
  weston --backend=headless-backend.so --socket=wayland-patchy --idle-time=0 > /out/weston.log 2>&1 &
  weston_pid=$!
  sleep 3
  if WAYLAND_DISPLAY=wayland-patchy QT_QPA_PLATFORM=wayland timeout 180 "$APP" --screenshot /out/wayland.png \
       > /out/wayland-console.txt 2>&1 && [ -s /out/wayland.png ]; then
    echo "ok"
  else
    fail "wayland window"; cat /out/wayland-console.txt /out/weston.log
  fi
  kill "$weston_pid" 2>/dev/null
else
  echo "skipped (no weston package)"
fi

echo "-- 4. connector"
if timeout 120 "$APP" patchy-mcp --check > /out/mcp-check.txt 2>&1; then
  echo "ok"
else
  fail "patchy-mcp --check"; cat /out/mcp-check.txt
fi
exit "$status"
