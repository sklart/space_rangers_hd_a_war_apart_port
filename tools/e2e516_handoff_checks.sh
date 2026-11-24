#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=build/matrixgame/e2e516-handoff-checks
mkdir -p "$out"
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_e2e516_battle_handoff.cpp -o "$out/handoff-host"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$out/handoff-host"
aarch64-linux-gnu-g++ -std=c++20 -O2 -static tests/test_e2e516_battle_handoff.cpp -o "$out/handoff-arm64"
qemu-aarch64 "$out/handoff-arm64"
printf '%s\n' 'E2E516 handoff ordering and failure recovery: host ASan/UBSan and ARM64/QEMU PASS'
