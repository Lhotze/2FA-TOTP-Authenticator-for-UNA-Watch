#!/usr/bin/env bash
# the PC tests of the authenticator: the codes and the readers of the keys against Python, the vault against Python, and the program (PIN, form, import files ...)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
G="$HERE/../Software/Apps/TouchGFX-GUI/gui"
SDK="${UNA_SDK:-$HERE/../../una-sdk}"
cd "$HERE"
python3 -I test_ta.py | grep -E "FAIL|passed|FAILED"
python3 -I test_vault.py | grep -E "FAIL|passed|FAILED"
g++ -std=c++17 -O2 -Wall -Wextra -I "$G/include" -I "$SDK/Libs/Header" -I "$HERE" "$HERE/test_app.cpp" "$G/src/ta/App.cpp" "$G/src/ta/Account.cpp" "$G/src/ta/Sha.cpp" "$G/src/ta/Vault.cpp" "$G/src/ta/Font.cpp" -o "$HERE/test_app" 2>&1 | grep -E "error" || true
"$HERE/test_app" "$@"
