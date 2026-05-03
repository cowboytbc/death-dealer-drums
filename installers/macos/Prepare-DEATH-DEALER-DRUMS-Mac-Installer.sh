#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DELIVERABLES_ROOT="$PROJECT_ROOT/DELIVERABLES/macOS"
OUTPUT_DIR="$PROJECT_ROOT/RELEASES/DEATH-DEALER-DRUMS-macOS-installer"

mkdir -p "$OUTPUT_DIR"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -e "$src" ]]; then
    rm -rf "$dst"
    ditto "$src" "$dst"
    echo "Copied: $src -> $dst"
  fi
}

copy_if_exists "$DELIVERABLES_ROOT/VST3/DEATH DEALER DRUMS.vst3" "$OUTPUT_DIR/DEATH DEALER DRUMS.vst3"
copy_if_exists "$DELIVERABLES_ROOT/AU/DEATH DEALER DRUMS.component" "$OUTPUT_DIR/DEATH DEALER DRUMS.component"
copy_if_exists "$DELIVERABLES_ROOT/Standalone/DEATH DEALER DRUMS.app" "$OUTPUT_DIR/DEATH DEALER DRUMS.app"

cp "$SCRIPT_DIR/Install-DEATH-DEALER-DRUMS-Mac.command" "$OUTPUT_DIR/Install-DEATH-DEALER-DRUMS-Mac.command"
chmod +x "$OUTPUT_DIR/Install-DEATH-DEALER-DRUMS-Mac.command"

if [[ -e "$PROJECT_ROOT/LOGO.png" ]]; then
  cp "$PROJECT_ROOT/LOGO.png" "$OUTPUT_DIR/LOGO.png"
fi

echo "macOS installer payload prepared at: $OUTPUT_DIR"
echo "Zip this folder for distribution, or run the .command directly on macOS."
