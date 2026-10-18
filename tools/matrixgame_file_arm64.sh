#!/usr/bin/env bash
# Actual Linux ARM64 CFile/PKG execution under QEMU; not Horizon/game execution.
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_root"
profile=file
if [[ "${1:-}" == --storage ]]; then profile=storage; shift; fi
out="$project_root/build/mg-core-arm64"
zlib_source="$project_root/build/matrixgame/zlib-1.3.2-source"
expected_zlib=da607da739fa6047df13e66a2af6b8bec7c2a498
mkdir -p "$out"
if [[ ! -d "$zlib_source/.git" ]]; then
  if [[ -e "$zlib_source" ]]; then
    echo "Refusing existing unmanaged zlib source directory" >&2
    exit 2
  fi
  git clone --depth 1 --branch v1.3.2 https://github.com/madler/zlib.git "$zlib_source"
fi
zgit() { git -c core.autocrlf=true -c safe.directory="$zlib_source" -C "$zlib_source" "$@"; }
[[ "$(zgit rev-parse HEAD)" == "$expected_zlib" ]] || { echo "zlib pin mismatch" >&2; exit 2; }
[[ -z "$(zgit status --porcelain --untracked-files=normal)" ]] || { echo "zlib source is dirty" >&2; exit 2; }
cmake -S "$zlib_source" -B "$out/zlib" \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_BUILD_TYPE=Release \
  -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF -DZLIB_INSTALL=OFF
cmake --build "$out/zlib" --target zlibstatic --parallel 2
python3 tools/matrixgame_core_prepare.py --output "$out/source" > "$out/source-provenance.json"
sources=(
  "$out/source/CBuf.cpp" "$out/source/CBuf_file.cpp"
  port/matrixgame/native-core/cbuf_utf16.cpp port/matrixgame/native-core/exception_native.cpp
  port/matrixgame/native-core/file_native.cpp port/switch/platform/package.cpp
)
if [[ "$profile" == storage ]]; then
  sources+=("$out/source/CStorage.cpp" "$out/source/CRC32.cpp" port/matrixgame/native-core/heap_native.cpp)
fi
aarch64-linux-gnu-g++ -std=c++20 -O2 -g -pthread \
  -I"$out/source" -Iport/matrixgame/native-core -Iport/switch/platform \
  -I"$out/zlib" -I"$zlib_source" \
  "tests/test_matrixgame_native_$profile.cpp" "${sources[@]}" \
  "$out/zlib/libz.a" -o "$out/$profile-contract"
python3 "tests/probe_matrixgame_native_$profile.py" --binary /usr/bin/qemu-aarch64 \
  --runner-binary "$out/$profile-contract" --qemu-sysroot /usr/aarch64-linux-gnu \
  --output "$out/$profile-fixtures" "$@"
[[ -z "$(zgit status --porcelain --untracked-files=normal)" ]] || { echo "zlib build changed source" >&2; exit 2; }
