#!/bin/sh
# Host-side syntax check of the sketch against stub Arduino/TFT_eSPI headers (no ESP32 toolchain needed).
# Usage: tools/check_syntax.sh <stubs_dir> [file.h|file.ino ...]   (default: the whole sketch)
STUBS="$1"; shift
DIR="$(cd "$(dirname "$0")/../firmware/CydGameHub" && pwd)"
if [ $# -eq 0 ]; then set -- "$DIR/CydGameHub.ino"; fi
for f in "$@"; do
  echo "== $f"
  g++ -std=gnu++11 -fsyntax-only -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter \
      -Werror=return-type -I"$STUBS" -I"$DIR" -include Arduino.h -x c++ "$f" || exit 1
done
echo OK
