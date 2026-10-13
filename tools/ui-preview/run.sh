#!/usr/bin/env bash
# Renders the PS4 launcher (externals/ps4sup/status_screen.cpp) on this computer into PNGs, to check
# its layout without the console: tools/ui-preview/run.sh [out dir]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$HERE/out}"
mkdir -p "$OUT"
clang++ -std=c++20 ${OPT:--O1} ${ARCH:-} -w -I"$HERE/../../eden/externals/ps4sup" -I"$HERE/../../eden/externals/stb" "$HERE/render.cpp" -o "$OUT/render"
cd "$OUT" && ./render "$OUT"
# PPM -> PNG with macOS's sips, or ImageMagick elsewhere.
for f in *.ppm; do
    if command -v sips >/dev/null; then sips -s format png "$f" --out "${f%.ppm}.png" >/dev/null; else magick "$f" "${f%.ppm}.png"; fi
    rm "$f"
done
ls "$OUT"/*.png
