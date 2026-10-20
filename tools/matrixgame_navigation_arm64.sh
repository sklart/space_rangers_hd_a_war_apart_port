#!/usr/bin/env bash
# Original navigation + actual CPU resource loader on Linux ARM64/QEMU, not Switch.
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_root"
profile=navigation
if [[ "${1:-}" == --lifecycle ]]; then profile=lifecycle;shift;fi
if [[ "${1:-}" == --big-buffers ]]; then profile=big-buffers;shift;fi
out="$project_root/build/e2e515/mg-navigation-arm64"
zlib_source="$project_root/build/e2e515/deps/zlib-1.3.2"
expected_zlib=da607da739fa6047df13e66a2af6b8bec7c2a498
[[ -d "$zlib_source/.git" ]] || { echo "Prepare pinned zlib v1.3.2 in $zlib_source" >&2; exit 2; }
zgit() {
  if [[ -f '/mnt/c/Program Files/Git/cmd/git.exe' ]]; then
    '/mnt/c/Program Files/Git/cmd/git.exe' -C "$(wslpath -w "$zlib_source")" "$@"
  else
    git -C "$zlib_source" "$@"
  fi
}
[[ "$(zgit rev-parse HEAD | tr -d '\r')" == "$expected_zlib" ]] || { echo 'zlib pin mismatch' >&2; exit 2; }
[[ -z "$(zgit status --porcelain)" ]] || { echo 'zlib source is dirty' >&2; exit 2; }
mkdir -p "$out"
python3 tools/matrixgame_engine_prepare.py --output "$out/source" > "$out/source-provenance.json"
cmake -S "$zlib_source" -B "$out/zlib" \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_BUILD_TYPE=Release \
  -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF -DZLIB_INSTALL=OFF
cmake --build "$out/zlib" --target zlibstatic --parallel 2
core="$out/source/portable-core"
test=tests/test_matrixgame_navigation.cpp
if [[ "$profile" == lifecycle ]]; then test=tests/test_matrixgame_native_lifecycle.cpp;fi
extra_sources=()
if [[ "$profile" == big-buffers ]]; then
  test=tests/test_matrixgame_big_buffers.cpp
  extra_sources=(port/matrixgame/native-core/buffer_resources.cpp port/matrixgame/native-core/buffer_geometry.cpp port/matrixgame/native-core/vector_geometry.cpp)
fi
aarch64-linux-gnu-g++ -std=c++20 -O2 -g -pthread -UNDEBUG -D_RELDEBUG -ffp-contract=off \
  -I"$core" -I"$out/source/MatrixGame/src/Logic" \
  -Iport/matrixgame/native-core -Iport/switch/platform -I"$out/zlib" -I"$zlib_source" \
  -Iport/matrixgame/third_party/directxmath/Inc -Iport/matrixgame/third_party/sal \
  "$test" "$out/source/MatrixGame/src/Logic/MatrixRoadNetwork.cpp" \
  "$core/CBuf.cpp" "$core/CBuf_file.cpp" "$core/CStorage.cpp" "$core/CRC32.cpp" \
  "$core/CMain.cpp" "$core/Form.cpp" "$core/CReminder.cpp" \
  port/matrixgame/native-core/cbuf_utf16.cpp port/matrixgame/native-core/exception_native.cpp \
  port/matrixgame/native-core/file_native.cpp port/matrixgame/native-core/heap_native.cpp \
  port/switch/platform/package.cpp "${extra_sources[@]}" "$out/zlib/libz.a" -o "$out/$profile-contract"
qemu-aarch64 -L /usr/aarch64-linux-gnu "$out/$profile-contract" "$@"
[[ -z "$(zgit status --porcelain)" ]] || { echo 'zlib build modified source' >&2; exit 2; }
sha256sum "$out/$profile-contract" "$out/zlib/libz.a"
