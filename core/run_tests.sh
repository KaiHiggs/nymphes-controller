#!/bin/sh
# Build and run every codec test. No CMake, no JUCE, no third-party dependencies.
#   ./run_tests.sh [vendor-dir] [captured-device-dump.syx]
# Both optional paths are resolved against the directory this script is run FROM, not
# against core/ (the script changes directory to core/ before building). The second
# argument runs the corpus runner over real VARIABLE-LENGTH frames, which the vendor
# dumps alone cannot exercise (they are all exactly 1049 bytes).
#
# Without a vendor dir the vendor-corpus gate SKIPs, because the corpus is not ours to
# ship. WITH a vendor dir the two dumps must be present: a mistyped or unreadable path
# is an error, not a skip, so CI cannot go green having verified nothing.
set -e
CALLER=$(pwd)
# Resolve this script's own directory, including through a symlink (a wrapper in ~/bin
# pointing at the tree). macOS's BSD readlink rejects `readlink -f --` (illegal option)
# and older macOS has no -f at all, so a failure falls back to $0: invocation from the
# tree, or via a readlink -f-capable symlink, both work; nothing else is claimed.
SELF=$(readlink -f "$0" 2>/dev/null) || SELF=$0
[ -n "$SELF" ] || SELF=$0
cd "$(dirname "$SELF")"
mkdir -p ../build
# CXX may legitimately hold a wrapper plus a compiler ("ccache clang++"), so $CC is
# expanded unquoted on purpose: $(CC) is one shell word list, not one path.
CC=${CXX:-clang++}
FLAGS="-std=c++17 -O2 -Wall -Wextra -Iinclude"

if [ -n "$1" ]; then
  case "$1" in /*) VENDOR="$1" ;; *) VENDOR="$CALLER/$1" ;; esac
  for f in Nymphes_Factory_Presets_Factory_Bank.syx Nymphes_Factory_Presets_User_Bank.syx; do
    if [ ! -f "$VENDOR/$f" ]; then
      echo "FAIL vendor corpus incomplete: missing $VENDOR/$f" >&2
      echo "     both vendor dumps are required; a mistyped vendor dir must not skip" >&2
      exit 1
    fi
  done
  echo "== canonical codec self-test (vendor corpus: $VENDOR) =="
else
  VENDOR=""
  echo "== canonical codec self-test (SKIP: no vendor dir given - CRC anchor + negative cases only) =="
fi

$CC $FLAGS src/Codec.cpp tests/test_codec.cpp -o ../build/test_codec
../build/test_codec "$VENDOR"
echo
echo "== corpus runner =="
$CC $FLAGS src/Codec.cpp tests/devcheck.cpp -o ../build/devcheck
if [ -n "$2" ]; then
  case "$2" in /*) CORPUS="$2" ;; *) CORPUS="$CALLER/$2" ;; esac
  ../build/devcheck "$CORPUS"
else
  echo "(no device corpus given)"
fi
