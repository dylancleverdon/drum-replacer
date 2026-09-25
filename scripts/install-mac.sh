#!/bin/bash
# Installs or updates the Killroom VST3 on macOS.
#
#   curl -fsSL https://raw.githubusercontent.com/dylancleverdon/drum-replacer/HEAD/scripts/install-mac.sh | bash
#
# Run it again any time to update. The plugin's own "Update" button runs this same script.
#
# Options:
#   --dest DIR   VST3 folder to install into (default: ~/Library/Audio/Plug-Ins/VST3)
#   --tag TAG    release to install, e.g. v1.0.3 (default: the latest release)
#   --zip FILE   install from a local Killroom-macOS.zip instead of downloading
#   --log FILE   append all output to FILE (used by the plugin)

set -euo pipefail

REPO="${KILLROOM_REPO:-dylancleverdon/drum-replacer}"
ASSET="Killroom-macOS.zip"
BUNDLE="Killroom.vst3"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
TAG=""
ZIP=""
LOG=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dest) DEST="$2"; shift 2 ;;
        --tag)  TAG="$2";  shift 2 ;;
        --zip)  ZIP="$2";  shift 2 ;;
        --log)  LOG="$2";  shift 2 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [[ -n "$LOG" ]]; then
    exec >>"$LOG" 2>&1
fi

fail() {
    echo "Error: $*"
    exit 1
}

WORK="$(mktemp -d "${TMPDIR:-/tmp}/killroom.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

if [[ -z "$ZIP" ]]; then
    if [[ -n "$TAG" ]]; then
        URL="https://github.com/$REPO/releases/download/$TAG/$ASSET"
    else
        URL="https://github.com/$REPO/releases/latest/download/$ASSET"
    fi

    echo "Downloading ${TAG:-latest release}"
    /usr/bin/curl -fsSL --retry 3 --connect-timeout 15 -o "$WORK/$ASSET" "$URL" \
        || fail "download failed ($URL)"
    ZIP="$WORK/$ASSET"
fi

echo "Unpacking"
/usr/bin/ditto -x -k "$ZIP" "$WORK/unpacked" || fail "couldn't unpack $ZIP"
NEW="$WORK/unpacked/$BUNDLE"
[[ -d "$NEW" ]] || fail "$ASSET doesn't contain $BUNDLE"

# Files downloaded by a browser are quarantined, and Live refuses to load quarantined plugins.
/usr/bin/xattr -dr com.apple.quarantine "$NEW" 2>/dev/null || true

# Release builds are ad-hoc signed. Apple Silicon won't load unsigned code, so re-sign if needed.
if ! /usr/bin/codesign --verify --deep "$NEW" >/dev/null 2>&1; then
    /usr/bin/codesign --force --deep --sign - "$NEW" >/dev/null 2>&1 || fail "couldn't sign $BUNDLE"
fi

mkdir -p "$DEST" 2>/dev/null || true
[[ -w "$DEST" ]] || fail "can't write to $DEST"

TARGET="$DEST/$BUNDLE"

echo "Installing"
# Never overwrite the old bundle in place: a running copy of Live has it loaded, and changing
# those files under it can crash Live. Moving it aside keeps the loaded copy intact; it's
# deleted along with the temp folder, and Live picks up the new one on its next start.
if [[ -e "$TARGET" ]]; then
    mv "$TARGET" "$WORK/previous.vst3" || fail "couldn't move the old $BUNDLE out of the way"
fi

if ! /usr/bin/ditto "$NEW" "$TARGET"; then
    [[ -e "$WORK/previous.vst3" ]] && mv "$WORK/previous.vst3" "$TARGET"
    fail "couldn't copy $BUNDLE into $DEST"
fi

VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$TARGET/Contents/Info.plist" 2>/dev/null || echo unknown)"

if [[ "$DEST" != "/Library/Audio/Plug-Ins/VST3" && -e "/Library/Audio/Plug-Ins/VST3/$BUNDLE" ]]; then
    echo "Note: another copy exists in /Library/Audio/Plug-Ins/VST3. Delete it so Live doesn't load the old one."
fi

echo "Installed Killroom $VERSION to $TARGET"
echo "Restart Ableton Live to use it."
echo "KILLROOM_INSTALL_OK $VERSION"
