#!/usr/bin/env bash
# the pictures for the store: runs the program on the PC with made-up accounts and builds the screenshots and the animated picture
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
G="$HERE/../Software/Apps/TouchGFX-GUI/gui"
SDK="${UNA_SDK:-$HERE/../../una-sdk}"
OUT="${1:-$HERE/../Media}"
W="${TMPDIR:-/tmp}/authmedia.$$"
mkdir -p "$W/work" "$OUT"
g++ -std=c++17 -O2 -w -I "$G/include" -I "$SDK/Libs/Header" -I "$HERE" "$HERE/make_media.cpp" "$G/src/ta/App.cpp" "$G/src/ta/Account.cpp" "$G/src/ta/Sha.cpp" "$G/src/ta/Vault.cpp" "$G/src/ta/Font.cpp" -o "$W/make_media"
rm -rf "$OUT/raw"; mkdir -p "$OUT/raw"
"$W/make_media" "$W/work" "$OUT/raw"
python3 -I "$HERE/make_media.py" "$OUT/raw" "$OUT"
