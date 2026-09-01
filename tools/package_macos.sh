#!/bin/bash
# Assemble "Spirula Studio.app" from an already-built `spirula`, and
# optionally a .dmg holding it. macOS only; see docs/build.md#packaging.
#
#   bash tools/package_macos.sh [--build-dir DIR] [--out DIR]
#                               [--sign IDENTITY] [--dmg]
#
# The bundle carries one binary and its icon, which only works because MoltenVK
# and libomp are linked statically (cmake/SsVulkan.cmake,
# cmake/SsMacBundle.cmake) -- verified below, not assumed.
#
# --sign defaults to ad-hoc ("-"): enough for a Mac the app is copied to
# directly, not enough to survive the quarantine flag a download attaches.

set -eu

if [ "$(uname)" != "Darwin" ]; then
    echo "package_macos.sh: macOS only (this is $(uname))" >&2
    exit 1
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT/build"
OUT_DIR=""
IDENTITY="-"
MAKE_DMG=0

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD_DIR=$2; shift 2 ;;
        --out)       OUT_DIR=$2; shift 2 ;;
        --sign)      IDENTITY=$2; shift 2 ;;
        --dmg)       MAKE_DMG=1; shift ;;
        -h|--help)   sed -n '2,13p' "$0" | sed 's/^#\{1\} \{0,1\}//'; exit 0 ;;
        *) echo "package_macos.sh: unknown argument $1" >&2; exit 1 ;;
    esac
done
[ -n "$OUT_DIR" ] || OUT_DIR=$BUILD_DIR

EXE="$BUILD_DIR/spirula"
if [ ! -x "$EXE" ]; then
    echo "package_macos.sh: no $EXE -- build with -DSS_BUILD_GUI=ON first" >&2
    exit 1
fi
# The dispatcher lists only the tools this build has, and `gui --help` is not
# the way to ask: that argument reaches spirula_gui_main, which opens a window.
if ! "$EXE" --help 2>&1 | grep -q '^ *gui '; then
    echo "package_macos.sh: $EXE has no GUI; rebuild with -DSS_BUILD_GUI=ON" >&2
    exit 1
fi

VERSION=$("$EXE" --version 2>/dev/null | tr -d '[:space:]')
[ -n "$VERSION" ] || VERSION=dev
# CFBundleVersion must be dotted digits, and "dev" is what an untagged tree
# reports; the real string still goes in CFBundleGetInfoString below.
case "$VERSION" in
    [0-9]*) NUMERIC=$(echo "$VERSION" | sed 's/[^0-9.].*$//; s/\.$//') ;;
    *)      NUMERIC=0.0.0 ;;
esac
[ -n "$NUMERIC" ] || NUMERIC=0.0.0

APP="$OUT_DIR/Spirula Studio.app"
echo "==> $APP  (version $VERSION)"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cp "$EXE" "$APP/Contents/MacOS/spirula"

# A regional build (SS_FONT_CJK=sc|tc|jp|kr|all) ships its face beside the
# executable, and inside the bundle that is Contents/MacOS.
if [ -d "$BUILD_DIR/fonts" ]; then
    cp -R "$BUILD_DIR/fonts" "$APP/Contents/MacOS/fonts"
    echo "    bundled $(ls "$BUILD_DIR/fonts" | wc -l | tr -d ' ') font file(s)"
fi

# ---- icon ----------------------------------------------------------------
# sips and iconutil are in the base system, so this needs no Xcode.
ICONSET=$(mktemp -d)/AppIcon.iconset
mkdir -p "$ICONSET"
for spec in 16:16x16 32:16x16@2x 32:32x32 64:32x32@2x 128:128x128 \
            256:128x128@2x 256:256x256 512:256x256@2x 512:512x512 1024:512x512@2x; do
    px=${spec%%:*}
    name=${spec#*:}
    sips -z "$px" "$px" "$ROOT/assets/icon.png" \
         --out "$ICONSET/icon_$name.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"
rm -rf "$(dirname "$ICONSET")"

# ---- Info.plist ----------------------------------------------------------
# No CFBundleDocumentTypes on purpose. GLFW does not forward the Apple Event
# that carries a double-clicked file, so claiming .ply would open the app with
# nothing in it -- worse than not appearing in "Open With" at all.
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>              <string>Spirula Studio</string>
    <key>CFBundleExecutable</key>        <string>spirula</string>
    <key>CFBundleIdentifier</key>        <string>io.github.harry7557558.spirula-studio</string>
    <key>CFBundleVersion</key>           <string>$NUMERIC</string>
    <key>CFBundleShortVersionString</key><string>$NUMERIC</string>
    <key>CFBundleGetInfoString</key>     <string>Spirula Studio $VERSION</string>
    <key>CFBundleIconFile</key>          <string>AppIcon</string>
    <key>CFBundlePackageType</key>       <string>APPL</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>LSMinimumSystemVersion</key>    <string>11.0</string>
    <key>LSApplicationCategoryType</key> <string>public.app-category.graphics-design</string>
    <key>NSHighResolutionCapable</key>   <true/>
    <key>NSHumanReadableCopyright</key>  <string>GPLv3. See LICENSE.</string>
</dict>
</plist>
PLIST
plutil -lint "$APP/Contents/Info.plist" >/dev/null
printf 'APPL????' > "$APP/Contents/PkgInfo"

# ---- self-containment ----------------------------------------------------
STRAY=$(otool -L "$APP/Contents/MacOS/spirula" | tail -n +2 | awk '{print $1}' \
        | grep -v '^/usr/lib/' | grep -v '^/System/Library/' || true)
if [ -n "$STRAY" ]; then
    echo "package_macos.sh: the binary links libraries this bundle does not carry:" >&2
    echo "$STRAY" | sed 's/^/    /' >&2
    echo "  such a bundle runs only on this machine. libvulkan: rebuild with" >&2
    echo "  -DSS_MACOS_VULKAN=static (the default). Anything else: install its" >&2
    echo "  .a so cmake/SsMacBundle.cmake links that (brew install libomp)." >&2
    exit 1
fi

# ---- signature -----------------------------------------------------------
# Not --deep: it is deprecated, and there is one binary to sign anyway. Ad-hoc
# is also mandatory rather than cosmetic on Apple silicon -- an unsigned arm64
# binary is killed on exec, and copying the executable into place invalidates
# whatever signature it had.
if [ "$IDENTITY" = "-" ]; then
    codesign --force --sign - --timestamp=none "$APP"
else
    # A real identity is only worth having if the result can be notarized, and
    # that wants both of these -- a timestamp, and the hardened runtime.
    codesign --force --sign "$IDENTITY" --options runtime --timestamp "$APP"
fi
codesign --verify --strict "$APP"
echo "    signed ($IDENTITY) and verified"

if [ "$MAKE_DMG" = 1 ]; then
    DMG="$OUT_DIR/Spirula Studio.dmg"
    STAGE=$(mktemp -d)
    cp -R "$APP" "$STAGE/"
    ln -s /Applications "$STAGE/Applications"
    rm -f "$DMG"
    hdiutil create -quiet -volname "Spirula Studio" -srcfolder "$STAGE" \
                   -ov -format UDZO "$DMG"
    rm -rf "$STAGE"
    echo "==> $DMG  ($(du -h "$DMG" | cut -f1 | tr -d ' '))"
fi

echo "==> done ($(du -sh "$APP" | cut -f1 | tr -d ' '))"
