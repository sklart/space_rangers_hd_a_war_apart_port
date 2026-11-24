#!/usr/bin/env bash
set -euo pipefail
cd /mnt/d/repos/space_rangers_hd_a_war_apart_port
out=build/matrixgame/gpu-v17-check
mkdir -p "$out"
for test in gpu_upload_arena uniform_packet_exact command_state_cache; do
  includes=(-Iport/matrixgame/native-engine -Iport/matrixgame/native-core -Iport/matrixgame/deko -I/mnt/c/devkitPro/libnx/include)
  src="tests/test_matrixgame_${test}.cpp"
  g++ -std=c++20 -O1 -g -UNDEBUG -fno-pie -no-pie -fsanitize=address,undefined -fno-omit-frame-pointer "${includes[@]}" "$src" -o "$out/${test}-host"
  "$out/${test}-host"
  aarch64-linux-gnu-g++ -std=c++20 -O1 -g -UNDEBUG -fno-pie -no-pie "${includes[@]}" "$src" -o "$out/${test}-arm"
  qemu-aarch64 -L /usr/aarch64-linux-gnu "$out/${test}-arm"
done
