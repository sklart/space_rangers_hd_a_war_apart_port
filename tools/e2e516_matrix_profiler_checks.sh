#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=build/matrixgame/e2e516-matrix-profiler-checks-20261124-1
test ! -e "$out"
mkdir -p "$out"
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_matrixgame_frame_profiler.cpp -o "$out/test-host"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$out/test-host" 2>"$out/host.log"
test "$(wc -l <"$out/host.log")" -eq 4
grep -q 'exception component=DrawObjects' "$out/host.log"
grep -q 'component=Draw calls=1000' "$out/host.log"
aarch64-linux-gnu-g++ -std=c++20 -O2 -static tests/test_matrixgame_frame_profiler.cpp -o "$out/test-arm64"
qemu-aarch64 "$out/test-arm64" 2>"$out/arm64.log"
test "$(wc -l <"$out/arm64.log")" -eq 4
grep -q 'component=Draw calls=1000' "$out/arm64.log"
printf '%s\n' 'Aggregated per-component profiling, exception diagnostics and bounded output: host ASan/UBSan and ARM64/QEMU PASS'
