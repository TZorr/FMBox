#!/bin/bash
#
#  build.sh - configure and build FMBox in Release, then verify it.
#
#  "It builds" and "it measures correctly" are the same command here: a drum
#  synth whose hat is twice as short at 96 kHz or whose hits land on block
#  boundaries compiles perfectly well.
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

cd "$PROJECT_DIR"

cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR"

echo
echo "Measuring the engine (WAVs in build/sounds):"
echo
"$BUILD_DIR/FMBoxCheck_artefacts/Release/FMBoxCheck" "$BUILD_DIR/sounds"

echo
echo "Checking the processor and rendering the panel:"
echo
"$BUILD_DIR/EditorShot_artefacts/Release/EditorShot" "$BUILD_DIR/shots"

echo
echo "Built:"
ls -d "$BUILD_DIR/FMBox_artefacts/Release/"*/*.{vst3,component,app} 2>/dev/null || true
echo "Panel: $BUILD_DIR/shots/fmbox.png"
