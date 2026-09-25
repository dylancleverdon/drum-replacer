#!/bin/bash
# Builds a double-clickable macOS installer (.pkg) that puts Killroom.vst3 into
# ~/Library/Audio/Plug-Ins/VST3 for the current user, so no admin password is needed
# and the plugin's own Update button can replace it later.
#
#   installer/mac/build-pkg.sh path/to/Killroom.vst3 1.0.3 Killroom-macOS.pkg

set -euo pipefail

VST3="$1"
VERSION="$2"
OUT="$3"
ID="com.dylancleverdon.killroom.vst3"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/root" "$WORK/packages"
ditto "$VST3" "$WORK/root/Killroom.vst3"

# By default the macOS installer "relocates" a bundle to wherever it finds another copy
# with the same ID (a build folder, an old download...). Always install to the VST3 folder.
# (pkgbuild may not list .vst3 bundles at all, in which case there's nothing to turn off.)
pkgbuild --analyze --root "$WORK/root" "$WORK/components.plist"
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/components.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/components.plist"
    i=$((i + 1))
done

pkgbuild --root "$WORK/root" \
         --component-plist "$WORK/components.plist" \
         --identifier "$ID" \
         --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$WORK/packages/Killroom-vst3.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Killroom $VERSION</title>
    <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>
    <options customize="never" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <choices-outline>
        <line choice="killroom"/>
    </choices-outline>
    <choice id="killroom" title="Killroom VST3">
        <pkg-ref id="$ID"/>
    </choice>
    <pkg-ref id="$ID" version="$VERSION">Killroom-vst3.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/packages" "$OUT"
echo "Built $OUT"
