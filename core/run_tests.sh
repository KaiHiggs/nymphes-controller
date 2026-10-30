#!/bin/sh
# Build and run every codec test. No CMake, no JUCE, no third-party dependencies.
#   ./run_tests.sh [vendor-dir] [captured-device-dump.syx]
# The second optional argument runs the corpus runner over real VARIABLE-LENGTH frames,
# which the vendor dumps alone cannot exercise (they are all exactly 1049 bytes).
set -e
CALLER=$(pwd)
cd "$(dirname "$0")"
mkdir -p ../build
CC=${CXX:-clang++}
FLAGS="-std=c++17 -O2 -Wall -Wextra -Iinclude"
echo "== canonical codec self-test (147 vendor frames) =="
$CC $FLAGS src/Codec.cpp tests/test_codec.cpp -o ../build/test_codec
../build/test_codec "$1"
echo
echo "== corpus runner =="
$CC $FLAGS src/Codec.cpp tests/devcheck.cpp -o ../build/devcheck
if [ -n "$2" ]; then
  case "$2" in /*) CORPUS="$2" ;; *) CORPUS="$CALLER/$2" ;; esac
  ../build/devcheck "$CORPUS"
else
  echo "(no device corpus given)"
fi
