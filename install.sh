#!/usr/bin/env bash
# install.sh — put NymphesController.vst3 where Ableton Live finds it.
#
#   ./install.sh              build if needed, then install
#   ./install.sh --no-build   install an existing build only
#   ./install.sh --uninstall  remove the installed plugin
#
# Nothing else on the system is touched: no package manager, no framework, no
# plugin scanning, no permissions changes. The only write outside this tree is the
# VST3 bundle under ~/Library/Audio/Plug-Ins/VST3.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${HERE}/build"
ARTEFACT="${BUILD_DIR}/NymphesController_artefacts/Release/VST3/NymphesController.vst3"
DEST_DIR="${HOME}/Library/Audio/Plug-Ins/VST3"
DEST="${DEST_DIR}/NymphesController.vst3"

DO_BUILD=1
DO_UNINSTALL=0

for arg in "$@"; do
    case "$arg" in
        --no-build)  DO_BUILD=0 ;;
        --uninstall) DO_UNINSTALL=1 ;;
        -h|--help)
            sed -n '2,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            echo "install.sh: unknown option '$arg' (try --help)" >&2
            exit 2 ;;
    esac
done

say() { printf '%s\n' "$*"; }
die() { printf 'install.sh: %s\n' "$*" >&2; exit 1; }

# --- uninstall ---------------------------------------------------------------
if [ "$DO_UNINSTALL" -eq 1 ]; then
    if [ -d "$DEST" ]; then
        rm -rf "$DEST"
        say "removed  $DEST"
    else
        say "nothing to remove: $DEST does not exist"
    fi
    exit 0
fi

# --- build -------------------------------------------------------------------
if [ "$DO_BUILD" -eq 1 ]; then
    if [ ! -f "${HERE}/CMakeLists.txt" ]; then
        die "no CMakeLists.txt in $HERE — run this from the integration tree"
    fi

    if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
        say "configuring..."
        cmake -S "$HERE" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    fi

    say "building..."
    cmake --build "$BUILD_DIR" --parallel

    say "running the offline tests..."
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

# --- checks ------------------------------------------------------------------
[ -d "$ARTEFACT" ] || die "VST3 not found at:
  $ARTEFACT
Build it first (drop --no-build) or check that NymphesController_VST3 built."

EXE="${ARTEFACT}/Contents/MacOS/NymphesController"
[ -f "$EXE" ] || die "the VST3 bundle has no executable at $EXE"

# The bundle must carry a moduleinfo.json or hosts will list it and then fail to
# load it. This is the cheap check that catches a broken bundle before Live does.
if [ ! -f "${ARTEFACT}/Contents/Resources/moduleinfo.json" ]; then
    say "warning: no moduleinfo.json in the bundle; Live may list the plugin but fail to load it"
fi

# --- install -----------------------------------------------------------------
mkdir -p "$DEST_DIR"

if [ -d "$DEST" ]; then
    say "replacing existing $DEST"
    rm -rf "$DEST"
fi

# ditto preserves the bundle's structure, symlinks and extended attributes; cp -R
# on macOS does not always keep the code signature intact.
ditto "$ARTEFACT" "$DEST"

# --- verify ------------------------------------------------------------------
[ -d "$DEST" ] || die "install failed: $DEST does not exist"
[ -f "${DEST}/Contents/MacOS/NymphesController" ] || die "install failed: executable missing in $DEST"

say ""
say "installed  $DEST"
say ""
say "next:"
say "  1. In Ableton Live 12: Preferences > Plug-Ins > VST3 Plug-In Custom Folder"
say "     leave it at the default ($DEST_DIR), then press Rescan."
say "  2. Drop NymphesController on a MIDI track. The plugin opens the 'Nymphes'"
say "     CoreMIDI port itself, so no MIDI routing is needed in Live."
say "  3. Press 'Dump' in the Browser tab with the synth connected, or import a .syx,"
say "     to populate the 98-slot list."
say ""
say "uninstall with:  $0 --uninstall"
