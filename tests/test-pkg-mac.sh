#!/bin/bash
# CI test for the macOS .pkg installer: installs for the current user, checks the bundle
# landed in ~/Library/Audio/Plug-Ins/VST3 owned by that user (so the Update button can
# replace it), then runs the updater script over it.
#
#   tests/test-pkg-mac.sh Killroom-macOS.pkg Killroom-macOS.zip

set -euo pipefail

PKG="$1"
ZIP="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
BUNDLE="$DEST/Killroom.vst3"

rm -rf "$BUNDLE"

echo "== install the .pkg for the current user"
installer -pkg "$PKG" -target CurrentUserHomeDirectory
test -f "$BUNDLE/Contents/MacOS/Killroom" || { echo "plugin not installed to $DEST"; exit 1; }
codesign --verify --deep --strict "$BUNDLE"
xattr -p com.apple.quarantine "$BUNDLE" >/dev/null 2>&1 && { echo "installed plugin is quarantined"; exit 1; }

for path in "$DEST" "$BUNDLE" "$BUNDLE/Contents/MacOS/Killroom"; do
    owner="$(stat -f %Su "$path")"
    [[ "$owner" == "$(id -un)" ]] || { echo "$path is owned by $owner, so updates would fail"; exit 1; }
done

echo "== the Update button's installer can replace a .pkg install"
LOG="$(mktemp)"
bash "$HERE/../scripts/install-mac.sh" --zip "$ZIP" --dest "$DEST" --log "$LOG"
cat "$LOG"
tail -n 1 "$LOG" | grep -q '^KILLROOM_INSTALL_OK' || { echo "update over the .pkg install failed"; exit 1; }

rm -rf "$BUNDLE"
echo "pkg tests passed"
