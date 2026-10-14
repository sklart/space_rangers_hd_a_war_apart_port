#!/usr/bin/env bash
# Native Linux/WSL check of original GI/buffer decode, rescale, and repack.
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

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/port/switch/build/e2e5-host-graphbuf"
mkdir -p "$build"
python3 - "$root" "$build" <<'PY'
import sys
from pathlib import Path
root, build = map(Path, sys.argv[1:])
sys.path.insert(0, str(root / "tools"))
from e2e_source_overrides import generated_source
generated_source(root / "upstream/cpp/src/GR_GraphBuf.cpp", build / "GR_GraphBuf.cpp")
PY
cmake -S "$root/upstream/okgf" -B "$build/okgf" \
  -DOKGF_MATH_BACKEND=NATIVE -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build/okgf" --parallel 2

common=(-std=gnu++20 -D__SWITCH__ -ffunction-sections -fdata-sections
        -Wno-invalid-offsetof -I"$root/port/switch/platform" -I"$root/upstream/cpp/src"
        -I"$root/upstream/cpp/runtime" -I"$root/upstream/okgf/include")
link=(-Wl,--gc-sections -L"$build/okgf" -lokgf
      -Wl,-rpath,"$build/okgf")

g++ "${common[@]}" -O0 -c "$build/GR_GraphBuf.cpp" \
  -o "$build/GR_GraphBuf.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/GR_gi.cpp" \
  -o "$build/GR_gi.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/EC_Buf.cpp" \
  -o "$build/EC_Buf.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/EC_Struct.cpp" \
  -o "$build/EC_Struct.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/EC_Str.cpp" \
  -o "$build/EC_Str.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/EC_CacheGI.cpp" \
  -o "$build/EC_CacheGI.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/EC_Cache.cpp" \
  -o "$build/EC_Cache.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/runtime/runtime.cpp" \
  -o "$build/runtime.o"
g++ "${common[@]}" -O0 "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/GR_GraphBuf.o" "$build/GR_gi.o" "$build/EC_Buf.o" \
  "$build/EC_Struct.o" "$build/runtime.o" "${link[@]}" -o "$build/graphbuf"
g++ "${common[@]}" -O0 -DE2E5_ORIGINAL_CACHEGI \
  "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/EC_CacheGI.o" "$build/EC_Cache.o" "$build/EC_Str.o" "$build/GR_GraphBuf.o" \
  "$build/GR_gi.o" "$build/EC_Buf.o" "$build/EC_Struct.o" \
  "$build/runtime.o" "${link[@]}" -o "$build/cachegi"
python3 "$root/tools/e2e5_gamesettings_rescale.py" "${source_args[@]}" \
  --okgf-lib "$build/okgf/libokgf.so" --original-graphbuf "$build/graphbuf" \
  --original-cachegi "$build/cachegi"

# The translated Pascal class hierarchy has RTTI references outside this
# fixture; retain ASan and UBSan checks while excluding only vptr checks.
san=(-O1 -g -fsanitize=address,undefined -fno-sanitize=vptr
     -fno-omit-frame-pointer)
g++ "${common[@]}" "${san[@]}" -c "$build/GR_GraphBuf.cpp" \
  -o "$build/GR_GraphBuf_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/GR_gi.cpp" \
  -o "$build/GR_gi_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/EC_Buf.cpp" \
  -o "$build/EC_Buf_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/EC_Struct.cpp" \
  -o "$build/EC_Struct_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/EC_Str.cpp" \
  -o "$build/EC_Str_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/EC_CacheGI.cpp" \
  -o "$build/EC_CacheGI_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/EC_Cache.cpp" \
  -o "$build/EC_Cache_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/runtime/runtime.cpp" \
  -o "$build/runtime_sanitized.o"
g++ "${common[@]}" "${san[@]}" "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/GR_GraphBuf_sanitized.o" "$build/GR_gi_sanitized.o" \
  "$build/EC_Buf_sanitized.o" "$build/EC_Struct_sanitized.o" \
  "$build/runtime_sanitized.o" \
  "${link[@]}" -o "$build/graphbuf-sanitized"
g++ "${common[@]}" "${san[@]}" -DE2E5_ORIGINAL_CACHEGI \
  "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/EC_CacheGI_sanitized.o" "$build/EC_Cache_sanitized.o" \
  "$build/EC_Str_sanitized.o" \
  "$build/GR_GraphBuf_sanitized.o" "$build/GR_gi_sanitized.o" \
  "$build/EC_Buf_sanitized.o" "$build/EC_Struct_sanitized.o" \
  "$build/runtime_sanitized.o" "${link[@]}" -o "$build/cachegi-sanitized"
python3 "$root/tools/e2e5_gamesettings_rescale.py" "${source_args[@]}" \
  --okgf-lib "$build/okgf/libokgf.so" --original-graphbuf "$build/graphbuf-sanitized" \
  --original-cachegi "$build/cachegi-sanitized"
