#!/bin/bash
#
#  install.sh - put the built plugins where the hosts look, and make sure the
#  hosts actually pick up the new build.
#
#  The cache flush is the part that matters. macOS keeps a registry of audio
#  units and Logic keeps a scan cache on top of it; replacing the bundle on
#  disk does not by itself make either let go of the old one - and testing a
#  binary from before the fix looks exactly like the fix not working.
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTEFACTS="$PROJECT_DIR/build/FMBox_artefacts/Release"

AU_SOURCE="$ARTEFACTS/AU/FMBox.component"
VST3_SOURCE="$ARTEFACTS/VST3/FMBox.vst3"
APP_SOURCE="$ARTEFACTS/Standalone/FMBox.app"

AU_DEST="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3"
APP_DEST="/Applications"

if [[ ! -d "$VST3_SOURCE" || ! -d "$AU_SOURCE" ]]; then
    echo "No build found at $ARTEFACTS - run Scripts/build.sh first." >&2
    exit 1
fi

# Without the AudioComponents entry macOS does not register the unit, does not
# complain, and the plugin simply never appears in the host.
if ! /usr/libexec/PlistBuddy -c "Print :AudioComponents:0:subtype" \
        "$AU_SOURCE/Contents/Info.plist" >/dev/null 2>&1; then
    echo "The AU's Info.plist has no AudioComponents entry - rebuild from scratch:" >&2
    echo "    rm -rf build/FMBox_artefacts && Scripts/build.sh" >&2
    exit 1
fi

# Refuse to install a build older than its sources. Verification/ and the
# Markdown notes are left out on purpose: neither is in the plugin.
stale="$(find "$PROJECT_DIR/Source" "$PROJECT_DIR/CMakeLists.txt" -type f ! -name '*.md' \
             -newer "$AU_SOURCE/Contents/MacOS/FMBox" -print -quit 2>/dev/null || true)"
if [[ -n "$stale" ]]; then
    echo "The build is older than ${stale#"$PROJECT_DIR"/} - run Scripts/build.sh first." >&2
    exit 1
fi

if pgrep -xq "Logic Pro"; then
    echo "Logic Pro is running. Quit it first: it holds the old plugin open and" >&2
    echo "will not rescan while it is." >&2
    exit 1
fi

mkdir -p "$AU_DEST" "$VST3_DEST"
rm -rf "$AU_DEST/FMBox.component" "$VST3_DEST/FMBox.vst3"
cp -R "$AU_SOURCE"   "$AU_DEST/"
cp -R "$VST3_SOURCE" "$VST3_DEST/"
echo "Installed AU and VST3 to $AU_DEST and $VST3_DEST"

if [[ -d "$APP_SOURCE" ]]; then
    rm -rf "$APP_DEST/FMBox.app"
    cp -R "$APP_SOURCE" "$APP_DEST/"
    echo "Installed the standalone app to $APP_DEST"
fi

killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1

echo
echo "Validating the Audio Unit:"
auval -v aumu Fmbx Tzor

echo
echo "What a host will find:"
auval -a 2>/dev/null | grep -i "Fmbx" || echo "  (nothing - the AU did not register)"
