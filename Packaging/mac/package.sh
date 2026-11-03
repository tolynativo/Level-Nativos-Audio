#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build-mac}"
DIST="$ROOT/dist"
mkdir -p "$DIST/vstroot/Library/Audio/Plug-Ins/VST3" "$DIST/auroot/Library/Audio/Plug-Ins/Components" "$DIST/approot/Applications"
VST="$(find "$BUILD" -name 'Level.vst3' -type d -print -quit)"
AU="$(find "$BUILD" -name 'Level.component' -type d -print -quit)"
APP="$(find "$BUILD" -name 'Level.app' -type d -print -quit)"
test -n "$VST" && test -n "$AU" && test -n "$APP"
cp -R "$VST" "$DIST/vstroot/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU" "$DIST/auroot/Library/Audio/Plug-Ins/Components/"
cp -R "$APP" "$DIST/approot/Applications/"
for TYPE in vstroot auroot approot;do
 mkdir -p "$DIST/$TYPE/Library/Application Support/Nativos Audio/Level"
 cp "$ROOT/Resources/Level-Manual.html" "$DIST/$TYPE/Library/Application Support/Nativos Audio/Level/"
done
pkgbuild --root "$DIST/vstroot" --identifier com.nativosaudio.level.vst3 --version 0.4.0 --install-location / "$DIST/Level-VST3.pkg"
pkgbuild --root "$DIST/auroot" --identifier com.nativosaudio.level.au --version 0.4.0 --install-location / "$DIST/Level-AU.pkg"
pkgbuild --root "$DIST/approot" --identifier com.nativosaudio.level.app --version 0.4.0 --install-location / "$DIST/Level-Standalone.pkg"
(cd "$DIST" && productbuild --distribution "$ROOT/Packaging/mac/distribution.xml" --resources "$ROOT/Packaging/mac" --package-path "$DIST" Level-Mac-Universal-UNSIGNED.pkg)
test -s "$DIST/Level-Mac-Universal-UNSIGNED.pkg"
