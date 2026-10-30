#!/bin/sh
# Capture the NymphesController standalone window to a PNG.
# Usage: tools/capture-ui.sh [path/to/NymphesController.app] [out.png]
# Requires macOS Screen Recording + Accessibility for the terminal (kitty).
set -eu
APP=${1:-build/NymphesController_artefacts/Release/Standalone/NymphesController.app}
OUT=${2:-/tmp/nymphes-ui-$(date +%s).png}
LOCK=${TMPDIR:-/tmp}/nymphes-ui-capture.lock
tries=0
while ! mkdir "$LOCK" 2>/dev/null; do
  tries=$((tries + 1))
  if [ "$tries" -gt 120 ]; then echo "capture-ui: lock $LOCK is busy" >&2; exit 1; fi
  sleep 0.5
done
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT
# One window at a time: stop any previous instance, then open the build we were asked to capture.
osascript -e 'tell application "NymphesController" to quit' >/dev/null 2>&1 || true
sleep 0.5
open "$APP"
sleep 1
osascript -e 'tell application "System Events" to set frontmost of process "NymphesController" to true' >/dev/null
sleep 0.5
BOUNDS=$(osascript -e 'tell application "System Events" to tell process "NymphesController" to get {position, size} of window 1')
X=$(printf '%s' "$BOUNDS" | awk -F', ' '{print $1}')
Y=$(printf '%s' "$BOUNDS" | awk -F', ' '{print $2}')
W=$(printf '%s' "$BOUNDS" | awk -F', ' '{print $3}')
H=$(printf '%s' "$BOUNDS" | awk -F', ' '{print $4}')
if [ -z "${X:-}" ] || [ -z "${W:-}" ]; then echo "capture-ui: could not read the window bounds: $BOUNDS" >&2; exit 1; fi
if ! screencapture -x -R"$X,$Y,$W,$H" "$OUT" 2>/dev/null; then
  echo "capture-ui: screencapture failed. Grant Screen Recording to the terminal in System Settings > Privacy & Security." >&2
  exit 1
fi
echo "$OUT"
