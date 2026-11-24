#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=build/matrixgame/e2e516-frame-trace-checks-20261124-1
test ! -e "$out"
mkdir -p "$out"
g++ -std=c++20 -O1 -g -rdynamic tests/test_rangers_com_throw_trace.cpp port/matrixgame/native-engine/rangers_com_throw_trace.cpp -Wl,--wrap=__cxa_throw -o "$out/trace-host"
"$out/trace-host" >"$out/host.out" 2>"$out/host.err"
test "$(grep -c COM_NIL_TRACE "$out/host.err")" = 1
test "$(grep -c NATIVE_FRAME_TRACE "$out/host.err")" = 1
! grep -q 'unrelated failure' "$out/host.err"
aarch64-linux-gnu-g++ -std=c++20 -O1 -g -static tests/test_rangers_com_throw_trace.cpp port/matrixgame/native-engine/rangers_com_throw_trace.cpp -Wl,--wrap=__cxa_throw -o "$out/trace-arm64"
qemu-aarch64 "$out/trace-arm64" >"$out/arm64.out" 2>"$out/arm64.err"
test "$(grep -c COM_NIL_TRACE "$out/arm64.err")" = 1
test "$(grep -c NATIVE_FRAME_TRACE "$out/arm64.err")" = 1
! grep -q 'unrelated failure' "$out/arm64.err"
printf '%s\n' 'Exception filtering and stack trace: host and ARM64/QEMU PASS'
