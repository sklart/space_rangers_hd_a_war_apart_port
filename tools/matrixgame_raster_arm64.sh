#!/usr/bin/env bash
# Native state/register contract only; no GPU or MatrixGame battle.
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_root"
out=build/mg-raster-arm64
mkdir -p "$out"
aarch64-linux-gnu-g++ -std=c++20 -O2 -g tests/test_matrixgame_raster_state.cpp -o "$out/contract"
python3 tests/probe_matrixgame_raster_state.py --binary /usr/bin/qemu-aarch64 \
  --runner-binary "$out/contract" --qemu-sysroot /usr/aarch64-linux-gnu --output "$out/fixtures"
