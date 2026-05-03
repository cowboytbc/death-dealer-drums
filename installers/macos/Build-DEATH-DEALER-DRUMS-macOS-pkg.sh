#!/bin/bash
set -euo pipefail

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

RELEASE_DIR="$PROJECT_ROOT/release"
MAC_RELEASE_DIR="$RELEASE_DIR/macos"
PKG_STAGING="$PROJECT_ROOT/pkg-staging"
PKG_COMPONENTS="$PROJECT_ROOT/pkg-components"
OUTPUT_PKG="$RELEASE_DIR/DEATH-DEALER-DRUMS-macOS-${VERSION}.pkg"

mkdir -p "$MAC_RELEASE_DIR" "$PKG_STAGING" "$PKG_COMPONENTS" "$RELEASE_DIR"

# Prefer DELIVERABLES copies, fall back to build artefacts
find_first() {
  local pattern="$1"
  find "$PROJECT_ROOT" -type d -path "$pattern" 2>/dev/null | head -n 1
}

VST3_PATH=""
AU_PATH=""
APP_PATH=""

if [[ -d "$PROJECT_ROOT/DELIVERABLES/macOS/VST3" ]]; then
  VST3_PATH="$(find "$PROJECT_ROOT/DELIVERABLES/macOS/VST3" -maxdepth 1 -name '*.vst3' | head -n 1 || true)"
fi
if [[ -d "$PROJECT_ROOT/DELIVERABLES/macOS/AU" ]]; then
  AU_PATH="$(find "$PROJECT_ROOT/DELIVERABLES/macOS/AU" -maxdepth 1 -name '*.component' | head -n 1 || true)"
fi
if [[ -d "$PROJECT_ROOT/DELIVERABLES/macOS/Standalone" ]]; then
  APP_PATH="$(find "$PROJECT_ROOT/DELIVERABLES/macOS/Standalone" -maxdepth 1 -name '*.app' | head -n 1 || true)"
fi

[[ -z "$VST3_PATH" ]] && VST3_PATH="$(find_first '*/build/*_artefacts/Release/VST3/*.vst3')"
[[ -z "$AU_PATH"   ]] && AU_PATH="$(find_first '*/build/*_artefacts/Release/AU/*.component')"
[[ -z "$APP_PATH"  ]] && APP_PATH="$(find_first '*/build/*_artefacts/Release/Standalone/*.app')"

echo "VST3: $VST3_PATH"
echo "AU:   $AU_PATH"
echo "APP:  $APP_PATH"

if [[ -z "$VST3_PATH" || ! -d "$VST3_PATH" ]]; then
  echo "Error: VST3 bundle not found. Build macOS VST3 first." >&2
  exit 1
fi
if [[ -z "$AU_PATH" || ! -d "$AU_PATH" ]]; then
  echo "Error: AU bundle not found. Build macOS AU first." >&2
  exit 1
fi
if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
  echo "Error: Standalone app not found. Build macOS Standalone first." >&2
  exit 1
fi

# Collect for signing/notarization workflows if needed
rm -rf "$MAC_RELEASE_DIR/DEATH DEALER DRUMS.vst3" "$MAC_RELEASE_DIR/DEATH DEALER DRUMS.component" "$MAC_RELEASE_DIR/DEATH DEALER DRUMS.app"
cp -R "$VST3_PATH" "$MAC_RELEASE_DIR/"
cp -R "$AU_PATH" "$MAC_RELEASE_DIR/"
cp -R "$APP_PATH" "$MAC_RELEASE_DIR/"

# Stage install roots expected by pkgbuild
rm -rf "$PKG_STAGING/vst3" "$PKG_STAGING/au" "$PKG_STAGING/standalone"
mkdir -p "$PKG_STAGING/vst3/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_STAGING/au/Library/Audio/Plug-Ins/Components"
mkdir -p "$PKG_STAGING/standalone/Applications"

cp -R "$MAC_RELEASE_DIR/$(basename "$VST3_PATH")" "$PKG_STAGING/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$MAC_RELEASE_DIR/$(basename "$AU_PATH")" "$PKG_STAGING/au/Library/Audio/Plug-Ins/Components/"
cp -R "$MAC_RELEASE_DIR/$(basename "$APP_PATH")" "$PKG_STAGING/standalone/Applications/"

# Build component pkgs
rm -f "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-vst3.pkg" "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-au.pkg" "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-standalone.pkg"

pkgbuild --root "$PKG_STAGING/vst3" \
         --identifier com.infernotones.deathdealerdrums.vst3 \
         --version "$VERSION" \
         "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-vst3.pkg"

pkgbuild --root "$PKG_STAGING/au" \
         --identifier com.infernotones.deathdealerdrums.au \
         --version "$VERSION" \
         "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-au.pkg"

pkgbuild --root "$PKG_STAGING/standalone" \
         --identifier com.infernotones.deathdealerdrums.standalone \
         --version "$VERSION" \
         "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-standalone.pkg"

# Build final product pkg
rm -f "$OUTPUT_PKG"
productbuild \
  --distribution "$SCRIPT_DIR/distribution.xml" \
  --package-path "$PKG_COMPONENTS" \
  --resources "$SCRIPT_DIR" \
  --version "$VERSION" \
  "$OUTPUT_PKG"

echo "Built macOS pkg: $OUTPUT_PKG"
