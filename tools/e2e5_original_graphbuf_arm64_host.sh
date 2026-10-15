#!/usr/bin/env bash
# Linux ARM64 component run under QEMU; this does not run Horizon or the game.
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <installed-game-root>|--synthetic" >&2
  exit 2
fi
if [[ $1 == --synthetic ]]; then
  source_args=(--synthetic)
else
  source_args=(--game-root "$1")
fi
for executable in aarch64-linux-gnu-gcc aarch64-linux-gnu-g++ qemu-aarch64 cmake; do
  command -v "$executable" >/dev/null || { echo "missing $executable" >&2; exit 2; }
done

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/port/switch/build/e2e5-host-arm64"
mkdir -p "$build/compatible"

# The x86 library supplies an independent data oracle. The ARM64 fixture
# compares the original methods against direct ARM64 OKGF byte for byte.
cmake -S "$root/upstream/okgf" -B "$build/native-okgf" \
  -DOKGF_MATH_BACKEND=NATIVE -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build/native-okgf" --parallel 2

cat > "$build/compatible/CMakeLists.txt" <<'CMAKE'
cmake_minimum_required(VERSION 3.20)
project(e2e5_arm64_okgf C)
set(OKGF_PORTABLE_MATH ON)
add_subdirectory("${OKGF_SOURCE}/vendor/softfloat" softfloat-build)
add_library(okgf SHARED
  "${OKGF_SOURCE}/src/math/precision.c"
  "${OKGF_SOURCE}/src/math/trig_x87.c"
  "${OKGF_SOURCE}/src/math/trig_emulated.c"
  "${OKGF_SOURCE}/src/rescale.c"
  "${OKGF_SOURCE}/src/pixels.c")
target_include_directories(okgf PUBLIC "${OKGF_SOURCE}/include" PRIVATE "${OKGF_SOURCE}/src")
target_compile_definitions(okgf PUBLIC OKGF_GAME_RELEASE=OKGF_GAME_SRHD OKGF_PORTABLE_MATH=1)
target_link_libraries(okgf PRIVATE okgf_softfloat m)
CMAKE
cmake -S "$build/compatible" -B "$build/compatible/cross" \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
  -DCMAKE_BUILD_TYPE=Release -DOKGF_SOURCE="$root/upstream/okgf"
cmake --build "$build/compatible/cross" --parallel 4

python3 - "$root" "$build" <<'PY'
import sys
from pathlib import Path
root, build = map(Path, sys.argv[1:])
sys.path.insert(0, str(root / "tools"))
from e2e_source_overrides import generated_source
generated_source(root / "upstream/cpp/src/GR_GraphBuf.cpp", build / "GR_GraphBuf.cpp")
PY

common=(-std=gnu++20 -D__SWITCH__ -O0 -ffunction-sections -fdata-sections
        -Wno-invalid-offsetof -I"$root/port/switch/platform" -I"$root/upstream/cpp/src"
        -I"$root/upstream/cpp/runtime" -I"$root/upstream/okgf/include")
for name in GR_GraphBuf GR_gi EC_Buf EC_Struct EC_Str EC_CacheGI EC_Cache; do
  source="$root/upstream/cpp/src/$name.cpp"
  [[ $name == GR_GraphBuf ]] && source="$build/GR_GraphBuf.cpp"
  aarch64-linux-gnu-g++ "${common[@]}" -c "$source" -o "$build/$name.o"
done
aarch64-linux-gnu-g++ "${common[@]}" -c \
  "$root/upstream/cpp/runtime/runtime.cpp" -o "$build/runtime.o"
aarch64-linux-gnu-g++ "${common[@]}" \
  "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/GR_GraphBuf.o" "$build/GR_gi.o" "$build/EC_Buf.o" \
  "$build/EC_Struct.o" "$build/runtime.o" \
  -Wl,--gc-sections -L"$build/compatible/cross" -lokgf \
  -Wl,-rpath,"$build/compatible/cross" -o "$build/graphbuf-effective-compatible"
aarch64-linux-gnu-g++ "${common[@]}" -DE2E5_ORIGINAL_CACHEGI \
  "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/EC_CacheGI.o" "$build/EC_Cache.o" "$build/EC_Str.o" "$build/GR_GraphBuf.o" \
  "$build/GR_gi.o" "$build/EC_Buf.o" "$build/EC_Struct.o" \
  "$build/runtime.o" -Wl,--gc-sections \
  -L"$build/compatible/cross" -lokgf \
  -Wl,-rpath,"$build/compatible/cross" -o "$build/cachegi-effective-compatible"

python3 "$root/tools/e2e5_gamesettings_rescale.py" "${source_args[@]}" \
  --okgf-lib "$build/native-okgf/libokgf.so" \
  --original-graphbuf "$build/graphbuf-effective-compatible" \
  --original-cachegi "$build/cachegi-effective-compatible" \
  --emulator "$(command -v qemu-aarch64)" --sysroot /usr/aarch64-linux-gnu
