#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build-mac}"
DIST="$ROOT/dist"
# Avoid accidentally bundling stale files from an earlier build.
rm -rf "$DIST/vstroot" "$DIST/auroot" "$DIST/approot"
mkdir -p "$DIST/vstroot/Library/Audio/Plug-Ins/VST3" "$DIST/auroot/Library/Audio/Plug-Ins/Components" "$DIST/approot/Applications"
VST="$(find "$BUILD" -name 'Level.vst3' -type d -print -quit)"
AU="$(find "$BUILD" -name 'Level.component' -type d -print -quit)"
APP="$(find "$BUILD" -name 'Level.app' -type d -print -quit)"
test -n "$VST" && test -n "$AU" && test -n "$APP"
ditto "$VST" "$DIST/vstroot/Library/Audio/Plug-Ins/VST3/Level.vst3"
ditto "$AU" "$DIST/auroot/Library/Audio/Plug-Ins/Components/Level.component"
ditto "$APP" "$DIST/approot/Applications/Level.app"
verify_bundle() {
  local source="$1" destination="$2" original staged
  original="$(find "$source/Contents/MacOS" -maxdepth 1 -type f -print -quit)"
  test -n "$original" || { echo "No executable in $source" >&2; exit 3; }
  staged="$destination/Contents/MacOS/$(basename "$original")"
  test -s "$staged" || { echo "Missing staged executable: $staged" >&2; exit 4; }
  cmp "$original" "$staged" || { echo "Installer executable mismatches approved UI build" >&2; exit 5; }
  lipo "$staged" -verify_arch arm64 x86_64 || { echo "Binary not universal: $staged" >&2; exit 6; }
  shasum -a 256 "$staged"
}
verify_bundle "$VST" "$DIST/vstroot/Library/Audio/Plug-Ins/VST3/Level.vst3"
verify_bundle "$AU" "$DIST/auroot/Library/Audio/Plug-Ins/Components/Level.component"
verify_bundle "$APP" "$DIST/approot/Applications/Level.app"
for TYPE in vstroot auroot approot;do
 mkdir -p "$DIST/$TYPE/Library/Application Support/Nativos Audio/Level"
 cp "$ROOT/Resources/Level-Manual.html" "$DIST/$TYPE/Library/Application Support/Nativos Audio/Level/"
done
pkgbuild --root "$DIST/vstroot" --identifier com.nativosaudio.level.vst3 --version 0.4.0 --install-location / "$DIST/Level-VST3.pkg"
pkgbuild --root "$DIST/auroot" --identifier com.nativosaudio.level.au --version 0.4.0 --install-location / "$DIST/Level-AU.pkg"
pkgbuild --root "$DIST/approot" --identifier com.nativosaudio.level.app --version 0.4.0 --install-location / "$DIST/Level-Standalone.pkg"
(cd "$DIST" && productbuild --distribution "$ROOT/Packaging/mac/distribution.xml" --resources "$ROOT/Packaging/mac" --package-path "$DIST" Level-Mac-Universal-UNSIGNED.pkg)
test -s "$DIST/Level-Mac-Universal-UNSIGNED.pkg"
