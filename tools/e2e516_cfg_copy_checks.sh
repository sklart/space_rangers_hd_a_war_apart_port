#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=build/matrixgame/e2e516-cfg-copy-checks-20261124-1
test ! -e "$out"
mkdir -p "$out"
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iport/switch/platform tests/test_e2e5_cfg_enosys.cpp -o "$out/cfg-host"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$out/cfg-host" "$out/host-data"
aarch64-linux-gnu-g++ -std=c++20 -O2 -static -Iport/switch/platform tests/test_e2e5_cfg_enosys.cpp -o "$out/cfg-arm64"
qemu-aarch64 "$out/cfg-arm64" "$out/arm64-data"
printf '%s\n' 'CFG exclusive byte copy, ENOSYS, existing-file and race checks: host ASan/UBSan and ARM64/QEMU PASS'
