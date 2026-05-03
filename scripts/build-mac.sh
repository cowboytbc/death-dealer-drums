#!/usr/bin/env bash
set -euo pipefail

echo "========================================================"
echo "  DEATH DEALER DRUMS - macOS Build"
echo "  INFERNO TONES"
echo "========================================================"

BUILD_DIR="build/macos"
DELIVERABLES_DIR="DELIVERABLES/macOS"

mkdir -p "$BUILD_DIR"

echo ""
echo "[1/3] Configuring with CMake..."
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0" \
    -DDDD_COPY_PLUGIN_AFTER_BUILD=ON

echo ""
echo "[2/3] Building Release (Universal Binary)..."
cmake --build "$BUILD_DIR" --config Release --parallel

echo ""
echo "[3/3] Done!"
echo ""
echo "Deliverables: $DELIVERABLES_DIR"
echo ""
echo "========================================================"
echo "  BUILD SUCCESSFUL - DEATH DEALER DRUMS"
echo "========================================================"
