#!/bin/bash
# usage: Linux/Tests/delete_clamp_test.sh [path/to/Engine.cpp]   (default: Linux/ibus/Engine.cpp)
set -e
root=$(cd "$(dirname "$0")/../.." && pwd); out=${TMPDIR:-/tmp}/vikey-delete-clamp
engine=${1:-$root/Linux/ibus/Engine.cpp}
mkdir -p "$out"
cmake -S "$root/Linux" -B "$out/b" -G Ninja -DVIKEY_IBUS=OFF >/dev/null && cmake --build "$out/b" --target vikey_engine >/dev/null
g++ -std=c++17 -w -O1 -DENGINE_SRC="\"$engine\"" -DVIKEY_DATADIR='"/usr/share/vikey"' \
  -I"$root/Linux/ibus" -I"$root/Engine" -I"$root/Engine/platforms" $(pkg-config --cflags ibus-1.0 gtk4 gstreamer-1.0) \
  "$root/Linux/Tests/delete_clamp_test.cpp" "$root/Linux/ibus/Core.cpp" "$root/Linux/ibus/Prefs.cpp" "$root/Linux/ibus/Crash.cpp" \
  "$out/b/libvikey_engine.a" $(pkg-config --libs ibus-1.0 gtk4 gstreamer-1.0) -o "$out/delete_clamp_test"
"$out/delete_clamp_test"
