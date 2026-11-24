#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=build/matrixgame/e2e516-stick-policy-20261124-2
test ! -e "$out"
mkdir -p "$out"
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_matrixgame_native_stick_policy.cpp -o "$out/test-host"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$out/test-host"
aarch64-linux-gnu-g++ -std=c++20 -O2 -static tests/test_matrixgame_native_stick_policy.cpp -o "$out/test-arm64"
qemu-aarch64 "$out/test-arm64"
printf '%s\n' 'Right-stick camera thresholds and diagonal directions: host ASan/UBSan and ARM64/QEMU PASS'
