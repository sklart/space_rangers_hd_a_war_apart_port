#!/usr/bin/env bash
# Real CVectorObject CPU methods under Linux ARM64/QEMU, never a Switch runtime.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:?fresh build output required}";shift
[[ "$out" == build/* && ! -e "$out" ]] || { echo 'Use a fresh build/ output' >&2;exit 2; }
zsrc="$PWD/build/e2e515/deps/zlib-1.3.2"
zlib="$PWD/build/e2e515/mg-navigation-arm64/zlib/libz.a"
matrix_git='/mnt/c/Program Files/Git/cmd/git.exe'
[[ "$("$matrix_git" -C "$(wslpath -w "$zsrc")" rev-parse HEAD | tr -d '\r')" == da607da739fa6047df13e66a2af6b8bec7c2a498 ]]
[[ -z "$("$matrix_git" -C "$(wslpath -w "$zsrc")" status --porcelain)" && -f "$zlib" ]]
mkdir -p "$out"
python3 tools/matrixgame_core_prepare.py --output "$out/source" > "$out/source-provenance.json"
core="$out/source"
sources=(tests/test_matrixgame_vector_object.cpp)
for name in VectorObjectCpu CBlockPar CStorage CBuf CBuf_file CRC32 CReminder;do sources+=("$core/$name.cpp");done
for name in vector_geometry buffer_geometry buffer_resources cbuf_utf16 exception_native file_native heap_native;do
    sources+=("port/matrixgame/native-core/$name.cpp")
done
sources+=(port/switch/platform/package.cpp)
command=(aarch64-linux-gnu-g++-15 -std=c++20 -O2 -g -pthread -UNDEBUG -D_RELDEBUG -ffp-contract=off
    -ffunction-sections -fdata-sections -Wl,--gc-sections
    -I"$core" -Iport/matrixgame/native-core -Iport/switch/platform -I"$zsrc" -Ibuild/e2e515/mg-navigation-arm64/zlib
    -Iport/matrixgame/third_party/directxmath/Inc -Iport/matrixgame/third_party/sal
    "${sources[@]}" "$zlib" -o "$out/vector-object-contract")
printf '%q ' "${command[@]}" > "$out/compile-command.txt"
"${command[@]}" > "$out/compile.log" 2>&1
models=();for model in "$@";do models+=(--model "$model");done
python3 tests/probe_matrixgame_vector_object.py --binary "$out/vector-object-contract" --qemu "${models[@]}" --output "$out/runtime"
sha256sum "$out/vector-object-contract" "$zlib" > "$out/sha256.txt"
