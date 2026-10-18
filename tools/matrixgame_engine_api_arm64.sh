#!/usr/bin/env bash
# Execute the actual prepared engine callback API; no battle or renderer is simulated.
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_root"
out="$project_root/build/mg-engine-api-arm64"
mkdir -p "$out"
python3 tools/matrixgame_core_prepare.py --output "$out/source" > "$out/source-provenance.json"
aarch64-linux-gnu-g++ -std=c++20 -O2 -g -pthread -UNDEBUG -ffp-contract=off \
  -I"$out/source" -Iport/matrixgame/native-core -Iport/matrixgame/native \
  -Iupstream/cpp/src -Iupstream/cpp/runtime \
  tests/test_matrixgame_rangers_bridge.cpp \
  port/matrixgame/native/rangers_bridge.cpp port/matrixgame/native/engine_callbacks.cpp \
  -o "$out/engine-api-contract"
qemu-aarch64 -L /usr/aarch64-linux-gnu "$out/engine-api-contract"
