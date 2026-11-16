#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
VERSION="${VERSION:-}"
if [[ -z "$VERSION" ]]; then
    VERSION_HEADER="$REPO_ROOT/SeiSeeMp/mainwindow.h"
    [[ -f "$VERSION_HEADER" ]] || {
        echo "Error: version header not found: $VERSION_HEADER" >&2
        exit 1
    }
    mapfile -t versions < <(
        sed -nE 's/^[[:space:]]*#[[:space:]]*define[[:space:]]+VERSION[[:space:]]+"([^"]+)"[[:space:]]*$/\1/p' \
            "$VERSION_HEADER"
    )
    ((${#versions[@]} == 1)) || {
        echo "Error: expected exactly one #define VERSION \"value\" in $VERSION_HEADER; found ${#versions[@]}." >&2
        exit 1
    }
    VERSION="${versions[0]}"
fi
SAFE_VERSION="${VERSION//[^A-Za-z0-9._-]/-}"
QMAKE="${QMAKE:-qmake}"
MAKE="${MAKE:-make}"
LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy}"
APPIMAGETOOL="${APPIMAGETOOL:-appimagetool}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
OUTPUT_DIR="$REPO_ROOT/dist/linux"
OUTPUT="$OUTPUT_DIR/SeiSee-$SAFE_VERSION-x86_64.AppImage"
if [[ -n "${SEISEE_BUILD_DIR:-}" ]]; then
    BUILD_DIR="$SEISEE_BUILD_DIR"
elif [[ -f /.dockerenv ]]; then
    BUILD_DIR="$REPO_ROOT/build/docker-release"
else
    BUILD_DIR="$REPO_ROOT/build/linux-package-release"
fi
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/seisee-appimage.XXXXXX")"
APP_DIR="$WORK_DIR/SeiSee.AppDir"
DESKTOP_FILE="$WORK_DIR/seisee.desktop"
MIME_FILE="$WORK_DIR/application-x-segy.xml"
ICON_FILE="$REPO_ROOT/SeiSeeMp/images/SeiSeeMp.png"
SEGY_ICON_FILE="$REPO_ROOT/SeiSeeMp/images/segyfile.png"
SEGY_SVG_FILE="$REPO_ROOT/SeiSeeMp/images/segyfile.svg"
EXECUTABLE=""

cleanup() {
    rm -rf -- "$WORK_DIR"
}
trap cleanup EXIT

fail() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

command -v "$QMAKE" >/dev/null 2>&1 || fail "qmake not found: $QMAKE"
command -v "$MAKE" >/dev/null 2>&1 || fail "make not found: $MAKE"
command -v "$LINUXDEPLOY" >/dev/null 2>&1 || fail "linuxdeploy not found: $LINUXDEPLOY"
command -v linuxdeploy-plugin-qt >/dev/null 2>&1 ||
    fail "linuxdeploy-plugin-qt must be available on PATH"
command -v "$APPIMAGETOOL" >/dev/null 2>&1 ||
    fail "appimagetool not found: $APPIMAGETOOL"
[[ "$(uname -m)" == "x86_64" ]] ||
    fail "This script currently packages x86_64 Linux builds only."

export PATH="$(dirname "$(command -v "$QMAKE")"):$PATH"
export QMAKE
mkdir -p "$BUILD_DIR" "$OUTPUT_DIR" "$APP_DIR/usr/bin" \
    "$APP_DIR/usr/share/applications" \
    "$APP_DIR/usr/share/mime/packages" \
    "$APP_DIR/usr/share/icons/hicolor/scalable/mimetypes" \
    "$APP_DIR/usr/share/icons/hicolor/256x256/mimetypes" \
    "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

cd "$BUILD_DIR"
"$QMAKE" "$REPO_ROOT/GxApps.pro" -r CONFIG+=release CONFIG-=debug
"$MAKE" distclean
"$QMAKE" "$REPO_ROOT/GxApps.pro" -r CONFIG+=release CONFIG-=debug
"$MAKE" -j"$JOBS"

for candidate in \
    "$BUILD_DIR/SeiSeeMp/release/SeiSeeMp" \
    "$BUILD_DIR/SeiSeeMp/SeiSeeMp"; do
    if [[ -x "$candidate" ]]; then
        EXECUTABLE="$candidate"
        break
    fi
done
[[ -n "$EXECUTABLE" ]] || fail "Built SeiSeeMp executable not found."
[[ -f "$ICON_FILE" ]] || fail "Application icon not found: $ICON_FILE"
install -m 755 "$EXECUTABLE" "$APP_DIR/usr/bin/SeiSeeMp"
install -m 644 "$ICON_FILE" "$APP_DIR/usr/share/icons/hicolor/256x256/apps/seisee.png"

cat > "$DESKTOP_FILE" <<'EOF'
[Desktop Entry]
Type=Application
Name=SeiSee
Comment=Seismic data viewer
Exec=SeiSeeMp %F
Icon=seisee
MimeType=application/x-segy;
Categories=Science;Education;
Terminal=false
EOF
install -m 644 "$DESKTOP_FILE" "$APP_DIR/usr/share/applications/seisee.desktop"

cat > "$MIME_FILE" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
    <mime-type type="application/x-segy">
        <comment>SEG-Y Seismic Data</comment>
        <glob pattern="*.sgy"/>
        <glob pattern="*.segy"/>
        <glob pattern="*.SGY"/>
        <glob pattern="*.SEGY"/>
        <generic-icon name="application-x-segy"/>
    </mime-type>
</mime-info>
EOF
install -m 644 "$MIME_FILE" "$APP_DIR/usr/share/mime/packages/application-x-segy.xml"

if [[ -f "$SEGY_SVG_FILE" ]]; then
    install -m 644 "$SEGY_SVG_FILE" "$APP_DIR/usr/share/icons/hicolor/scalable/mimetypes/application-x-segy.svg"
fi
if [[ -f "$SEGY_ICON_FILE" ]]; then
    install -m 644 "$SEGY_ICON_FILE" "$APP_DIR/usr/share/icons/hicolor/256x256/mimetypes/application-x-segy.png"
fi

run_appimage_tool() {
    APPIMAGE_EXTRACT_AND_RUN=1 "$@"
}

run_appimage_tool "$LINUXDEPLOY" \
    --appdir "$APP_DIR" \
    --executable "$APP_DIR/usr/bin/SeiSeeMp" \
    --desktop-file "$DESKTOP_FILE" \
    --icon-file "$ICON_FILE" \
    --plugin qt

run_appimage_tool "$APPIMAGETOOL" "$APP_DIR" "$OUTPUT"
printf 'Created AppImage: %s\n' "$OUTPUT"
