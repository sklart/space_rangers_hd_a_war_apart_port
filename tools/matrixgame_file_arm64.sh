#!/usr/bin/env bash
# Actual Linux ARM64 CPU resource/configuration contracts; not Horizon/game execution.
set -euo pipefail
project_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$project_root"
profile=file
case "${1:-}" in
  --storage) profile=storage; shift ;;
  --blockpar) profile=blockpar; shift ;;
  --math) profile=math; shift ;;
  --random) profile=random; shift ;;
  --vector) profile=vector; shift ;;
  --reminder) profile=reminder; shift ;;
  --bitmap) profile=bitmap; shift ;;
  --texture) profile=texture; shift ;;
  --texture-request) profile=texture_request; shift ;;
esac
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
extra_includes=()
extra_libraries=()
if [[ "$profile" == bitmap || "$profile" == texture || "$profile" == texture_request ]]; then
  png_source="$project_root/build/matrixgame/libpng-1.6.37-source"
  expected_png=a40189cf881e9f0db80511c382292a5604c3c3d1
  if [[ ! -d "$png_source/.git" ]]; then
    [[ ! -e "$png_source" ]] || { echo "Refusing unmanaged libpng source" >&2; exit 2; }
    git clone --depth 1 --branch v1.6.37 https://github.com/glennrp/libpng.git "$png_source"
  fi
  pgit() { git -c core.autocrlf=true -c safe.directory="$png_source" -C "$png_source" "$@"; }
  [[ "$(pgit rev-parse HEAD)" == "$expected_png" && -z "$(pgit status --porcelain)" ]] || { echo "libpng pin/clean check failed" >&2; exit 2; }
  cmake -S "$png_source" -B "$out/libpng" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_BUILD_TYPE=Release -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF \
    -DM_LIBRARY="$(aarch64-linux-gnu-gcc -print-file-name=libm.a)" \
    -DZLIB_INCLUDE_DIR="$zlib_source" -DZLIB_LIBRARY="$out/zlib/libz.a"
  cmake --build "$out/libpng" --target png_static --parallel 2
  extra_includes=(-I"$out/libpng" -I"$png_source")
  extra_libraries=("$out/libpng/libpng16.a")
  sources+=("$out/source/CBitmap.cpp" "$out/source/FilePNG.cpp")
  if [[ "$profile" == texture ]]; then sources+=(port/matrixgame/native-core/texture_asset.cpp); fi
  if [[ "$profile" == texture_request ]]; then
    sources+=(port/matrixgame/native-core/texture_asset.cpp port/matrixgame/native-core/texture_request.cpp
      "$out/source/TextureFlags.cpp" "$out/source/CBlockPar.cpp")
  fi
fi
if [[ "$profile" == texture || "$profile" == texture_request ]]; then
  jpeg_source="$project_root/build/matrixgame/libjpeg-turbo-2.1.2-source"
  expected_jpeg=ecf021bc0d6f435daacff7c35ccaeef0145df1b9
  if [[ ! -d "$jpeg_source/.git" ]]; then
    [[ ! -e "$jpeg_source" ]] || { echo "Refusing unmanaged JPEG source" >&2; exit 2; }
    git clone --depth 1 --branch 2.1.2 https://github.com/libjpeg-turbo/libjpeg-turbo.git "$jpeg_source"
  fi
  jgit() { git -c core.autocrlf=true -c safe.directory="$jpeg_source" -C "$jpeg_source" "$@"; }
  [[ "$(jgit rev-parse HEAD)" == "$expected_jpeg" && -z "$(jgit status --porcelain)" ]] || { echo "JPEG pin/clean check failed" >&2; exit 2; }
  cmake -S "$jpeg_source" -B "$out/jpeg" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF
  cmake --build "$out/jpeg" --target jpeg-static --parallel 2
  extra_includes+=(-I"$out/jpeg" -I"$jpeg_source")
  extra_libraries+=("$out/jpeg/libjpeg.a")
  sources+=(port/matrixgame/native-core/jpeg_texture.cpp)
fi
if [[ "$profile" != file ]]; then
  sources+=("$out/source/CStorage.cpp" "$out/source/CRC32.cpp" port/matrixgame/native-core/heap_native.cpp)
fi
if [[ "$profile" == blockpar ]]; then
  sources+=("$out/source/CBlockPar.cpp" "$out/source/CStorage_blockpar.cpp")
fi
if [[ "$profile" == random ]]; then
  sources+=("$out/source/random.cpp")
fi
if [[ "$profile" == vector ]]; then
  sources+=(port/matrixgame/native-core/vector_geometry.cpp)
fi
if [[ "$profile" == reminder ]]; then
  sources+=(port/matrixgame/native-core/vector_geometry.cpp "$out/source/CReminder.cpp")
fi
if [[ "$profile" == math ]]; then
  sources+=("$out/source/Math3D.cpp")
fi
aarch64-linux-gnu-g++ -std=c++20 -O2 -g -pthread -ffp-contract=off \
  -I"$out/source" -Iport/matrixgame/native-core -Iport/switch/platform \
  -Iport/matrixgame/third_party/directxmath/Inc -Iport/matrixgame/third_party/sal \
  -I"$out/zlib" -I"$zlib_source" \
  "${extra_includes[@]}" \
  "tests/test_matrixgame_native_$profile.cpp" "${sources[@]}" \
  "${extra_libraries[@]}" "$out/zlib/libz.a" -o "$out/$profile-contract"
python3 "tests/probe_matrixgame_native_$profile.py" --binary /usr/bin/qemu-aarch64 \
  --runner-binary "$out/$profile-contract" --qemu-sysroot /usr/aarch64-linux-gnu \
  --output "$out/$profile-fixtures" "$@"
[[ -z "$(zgit status --porcelain --untracked-files=normal)" ]] || { echo "zlib build changed source" >&2; exit 2; }
if [[ "$profile" == bitmap || "$profile" == texture || "$profile" == texture_request ]]; then [[ -z "$(pgit status --porcelain)" ]] || { echo "libpng source changed" >&2; exit 2; }; fi
if [[ "$profile" == texture || "$profile" == texture_request ]]; then [[ -z "$(jgit status --porcelain)" ]] || { echo "JPEG source changed" >&2; exit 2; }; fi
