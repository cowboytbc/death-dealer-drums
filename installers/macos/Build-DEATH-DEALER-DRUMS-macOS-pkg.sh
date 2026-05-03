#!/bin/bash
set -euo pipefail

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [[ -x "$SCRIPT_DIR/Generate-LOGO-icns.sh" ]]; then
  "$SCRIPT_DIR/Generate-LOGO-icns.sh"
fi

RELEASE_DIR="$PROJECT_ROOT/release"
MAC_RELEASE_DIR="$RELEASE_DIR/macos"
PKG_STAGING="$PROJECT_ROOT/pkg-staging"
PKG_COMPONENTS="$PROJECT_ROOT/pkg-components"
OUTPUT_PKG="$RELEASE_DIR/DEATH-DEALER-DRUMS-macOS-${VERSION}.pkg"
DIST_XML="$PKG_COMPONENTS/distribution.generated.xml"

mkdir -p "$MAC_RELEASE_DIR" "$PKG_STAGING" "$PKG_COMPONENTS" "$RELEASE_DIR"

# Resolve bundle paths across local + CI layouts.
find_bundle() {
  local exact_name="$1"
  shift
  local roots=("$@")
  local extension="${exact_name##*.}"
  local root
  local found

  for root in "${roots[@]}"; do
    [[ -d "$root" ]] || continue
    found="$(find "$root" -type d -name "$exact_name" 2>/dev/null | head -n 1 || true)"
    if [[ -n "$found" ]]; then
      printf '%s\n' "$found"
      return 0
    fi
  done

  for root in "${roots[@]}"; do
    [[ -d "$root" ]] || continue
    found="$(find "$root" -type d -name "*.${extension}" 2>/dev/null | head -n 1 || true)"
    if [[ -n "$found" ]]; then
      printf '%s\n' "$found"
      return 0
    fi
  done

  printf '%s\n' ""
}

VST3_PATH=""
AU_PATH=""
APP_PATH=""

SEARCH_ROOTS=(
  "$PROJECT_ROOT/DELIVERABLES/macOS/VST3"
  "$PROJECT_ROOT/DELIVERABLES/macOS/AU"
  "$PROJECT_ROOT/DELIVERABLES/macOS/Standalone"
  "$PROJECT_ROOT/DELIVERABLES/macOS"
  "$PROJECT_ROOT/release/macos"
  "$PROJECT_ROOT/RELEASES/DEATH-DEALER-DRUMS-macOS-installer-payload"
  "$PROJECT_ROOT/build"
)

VST3_PATH="$(find_bundle 'DEATH DEALER DRUMS.vst3' "${SEARCH_ROOTS[@]}")"
AU_PATH="$(find_bundle 'DEATH DEALER DRUMS.component' "${SEARCH_ROOTS[@]}")"
APP_PATH="$(find_bundle 'DEATH DEALER DRUMS.app' "${SEARCH_ROOTS[@]}")"

echo "VST3: $VST3_PATH"
echo "AU:   $AU_PATH"
echo "APP:  $APP_PATH"

if [[ -z "$VST3_PATH" || ! -d "$VST3_PATH" ]]; then
  echo "Error: VST3 bundle not found. Build macOS VST3 first." >&2
  exit 1
fi
if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
  echo "Error: Standalone app not found. Build macOS Standalone first." >&2
  exit 1
fi

HAS_AU=0
if [[ -n "$AU_PATH" && -d "$AU_PATH" ]]; then
  HAS_AU=1
fi

# Collect for signing/notarization workflows if needed
rm -rf "$MAC_RELEASE_DIR/DEATH DEALER DRUMS.vst3" "$MAC_RELEASE_DIR/DEATH DEALER DRUMS.component" "$MAC_RELEASE_DIR/DEATH DEALER DRUMS.app"
cp -R "$VST3_PATH" "$MAC_RELEASE_DIR/"
if [[ "$HAS_AU" -eq 1 ]]; then
  cp -R "$AU_PATH" "$MAC_RELEASE_DIR/"
fi
cp -R "$APP_PATH" "$MAC_RELEASE_DIR/"

# Stage install roots expected by pkgbuild
rm -rf "$PKG_STAGING/vst3" "$PKG_STAGING/au" "$PKG_STAGING/standalone"
mkdir -p "$PKG_STAGING/vst3/Library/Audio/Plug-Ins/VST3"
if [[ "$HAS_AU" -eq 1 ]]; then
  mkdir -p "$PKG_STAGING/au/Library/Audio/Plug-Ins/Components"
fi
mkdir -p "$PKG_STAGING/standalone/Applications"

cp -R "$MAC_RELEASE_DIR/$(basename "$VST3_PATH")" "$PKG_STAGING/vst3/Library/Audio/Plug-Ins/VST3/"
if [[ "$HAS_AU" -eq 1 ]]; then
  cp -R "$MAC_RELEASE_DIR/$(basename "$AU_PATH")" "$PKG_STAGING/au/Library/Audio/Plug-Ins/Components/"
fi
cp -R "$MAC_RELEASE_DIR/$(basename "$APP_PATH")" "$PKG_STAGING/standalone/Applications/"

# Build component pkgs
rm -f "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-vst3.pkg" "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-au.pkg" "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-standalone.pkg" "$DIST_XML"

pkgbuild --root "$PKG_STAGING/vst3" \
         --identifier com.infernoplugins.deathdealerdrums.vst3 \
         --version "$VERSION" \
         "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-vst3.pkg"

if [[ "$HAS_AU" -eq 1 ]]; then
  pkgbuild --root "$PKG_STAGING/au" \
           --identifier com.infernoplugins.deathdealerdrums.au \
           --version "$VERSION" \
           "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-au.pkg"
fi

pkgbuild --root "$PKG_STAGING/standalone" \
         --identifier com.infernoplugins.deathdealerdrums.standalone \
         --version "$VERSION" \
         "$PKG_COMPONENTS/DEATH-DEALER-DRUMS-standalone.pkg"

cat > "$DIST_XML" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
  <title>DEATH DEALER DRUMS</title>
  <organization>com.infernoplugins</organization>
  <domains enable_localSystem="true"/>
  <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>

  <choices-outline>
    <line choice="vst3"/>
EOF

if [[ "$HAS_AU" -eq 1 ]]; then
cat >> "$DIST_XML" <<EOF
    <line choice="au"/>
EOF
fi

cat >> "$DIST_XML" <<EOF
    <line choice="standalone"/>
  </choices-outline>

  <choice id="vst3" visible="true" selected="true"
          title="VST3 Plugin"
          description="Installs DEATH DEALER DRUMS.vst3 to /Library/Audio/Plug-Ins/VST3. Works in Reaper, Ableton Live, Cubase, Studio One, Logic Pro, and most modern DAWs.">
    <pkg-ref id="com.infernoplugins.deathdealerdrums.vst3"/>
  </choice>
EOF

if [[ "$HAS_AU" -eq 1 ]]; then
cat >> "$DIST_XML" <<EOF

  <choice id="au" visible="true" selected="true"
          title="Audio Unit (AU) Plugin"
          description="Installs DEATH DEALER DRUMS.component to /Library/Audio/Plug-Ins/Components. Required for Logic Pro and GarageBand.">
    <pkg-ref id="com.infernoplugins.deathdealerdrums.au"/>
  </choice>
EOF
fi

cat >> "$DIST_XML" <<EOF

  <choice id="standalone" visible="true" selected="true"
          title="Standalone Application"
          description="Installs DEATH DEALER DRUMS.app to /Applications. Run the drum instrument without a DAW.">
    <pkg-ref id="com.infernoplugins.deathdealerdrums.standalone"/>
  </choice>

  <pkg-ref id="com.infernoplugins.deathdealerdrums.vst3" auth="Root">DEATH-DEALER-DRUMS-vst3.pkg</pkg-ref>
EOF

if [[ "$HAS_AU" -eq 1 ]]; then
cat >> "$DIST_XML" <<EOF
  <pkg-ref id="com.infernoplugins.deathdealerdrums.au" auth="Root">DEATH-DEALER-DRUMS-au.pkg</pkg-ref>
EOF
fi

cat >> "$DIST_XML" <<EOF
  <pkg-ref id="com.infernoplugins.deathdealerdrums.standalone" auth="Root">DEATH-DEALER-DRUMS-standalone.pkg</pkg-ref>
</installer-gui-script>
EOF

# Build final product pkg
rm -f "$OUTPUT_PKG"
productbuild \
  --distribution "$DIST_XML" \
  --package-path "$PKG_COMPONENTS" \
  --resources "$SCRIPT_DIR" \
  --version "$VERSION" \
  "$OUTPUT_PKG"

echo "Built macOS pkg: $OUTPUT_PKG"
