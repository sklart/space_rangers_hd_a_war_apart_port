#!/usr/bin/env bash
set -euo pipefail
cd /mnt/d/repos/space_rangers_hd_a_war_apart_port
out=build/matrixgame/gpu-v22-check
mkdir -p "$out"
for test in index_validation_equivalence staging_draw command_state_cache vertex_marks weak_resource_cache; do
  includes=(-Iport/matrixgame/native-engine -Iport/matrixgame/native-core -Iport/matrixgame/deko)
  src="tests/test_matrixgame_${test}.cpp"
  sources=("$src")
  if [[ "$test" == index_validation_equivalence || "$test" == staging_draw ]]; then sources+=(port/matrixgame/native-core/buffer_resources.cpp); fi
  g++ -std=c++20 -O1 -g -UNDEBUG -fno-pie -no-pie -fsanitize=address,undefined -fno-omit-frame-pointer "${includes[@]}" "${sources[@]}" -o "$out/${test}-host"
  "$out/${test}-host"
  aarch64-linux-gnu-g++ -std=c++20 -O1 -g -UNDEBUG -fno-pie -no-pie "${includes[@]}" "${sources[@]}" -o "$out/${test}-arm"
  qemu-aarch64 -L /usr/aarch64-linux-gnu "$out/${test}-arm"
done
