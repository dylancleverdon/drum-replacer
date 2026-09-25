#!/bin/bash
# CI test for scripts/install-mac.sh: fresh install, then an update while another process
# has the installed plugin loaded (the situation when Live is open).
#
#   tests/test-installer-mac.sh Killroom-macOS.zip

set -euo pipefail

ZIP="$1"
HERE="$(cd "$(dirname "$0")" && pwd)"
INSTALLER="$HERE/../scripts/install-mac.sh"
DEST="$(mktemp -d)/VST3"
BINARY="$DEST/Killroom.vst3/Contents/MacOS/Killroom"
LOG="$(mktemp)"

echo "== fresh install"
bash "$INSTALLER" --zip "$ZIP" --dest "$DEST" --log "$LOG"
cat "$LOG"
tail -n 1 "$LOG" | grep -q '^KILLROOM_INSTALL_OK' || { echo "fresh install didn't report success"; exit 1; }
test -f "$BINARY" || { echo "plugin binary missing after install"; exit 1; }
codesign --verify --deep --strict "$DEST/Killroom.vst3"

echo "== update while the plugin is loaded"
python3 -c 'import ctypes, sys, time; ctypes.CDLL(sys.argv[1]); print("loaded", flush=True); time.sleep(120)' "$BINARY" &
HOLDER=$!
sleep 5
kill -0 "$HOLDER" || { echo "couldn't load the plugin binary"; exit 1; }

: > "$LOG"
bash "$INSTALLER" --zip "$ZIP" --dest "$DEST" --log "$LOG"
cat "$LOG"
tail -n 1 "$LOG" | grep -q '^KILLROOM_INSTALL_OK' || { echo "update didn't report success"; exit 1; }
test -f "$BINARY" || { echo "plugin binary missing after update"; exit 1; }
sleep 2
kill -0 "$HOLDER" || { echo "the process that had the plugin loaded died during the update"; exit 1; }
kill "$HOLDER"

echo "== installer rejects a bad zip"
echo "not a zip" > "$(dirname "$LOG")/bad.zip"
if bash "$INSTALLER" --zip "$(dirname "$LOG")/bad.zip" --dest "$DEST" >/dev/null 2>&1; then
    echo "installer accepted a broken zip"; exit 1
fi
test -f "$BINARY" || { echo "a failed install removed the existing plugin"; exit 1; }

echo "installer tests passed"
