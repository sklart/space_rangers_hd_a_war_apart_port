#!/usr/bin/env bash
# Native Linux/WSL release-backed check of the unchanged original GraphBuf method.
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
cmake -S "$root/upstream/okgf" -B "$build/okgf" \
  -DOKGF_MATH_BACKEND=NATIVE -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build/okgf" --parallel 2

common=(-std=gnu++20 -D__SWITCH__ -ffunction-sections -fdata-sections
        -Wno-invalid-offsetof -I"$root/upstream/cpp/src"
        -I"$root/upstream/cpp/runtime" -I"$root/upstream/okgf/include")
link=(-Wl,--gc-sections -L"$build/okgf" -lokgf
      -Wl,-rpath,"$build/okgf")

g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/GR_GraphBuf.cpp" \
  -o "$build/GR_GraphBuf.o"
g++ "${common[@]}" -O0 -c "$root/upstream/cpp/src/GR_gi.cpp" \
  -o "$build/GR_gi.o"
g++ "${common[@]}" -O0 "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/GR_GraphBuf.o" "$build/GR_gi.o" "${link[@]}" -o "$build/graphbuf"
python3 "$root/tools/e2e5_gamesettings_rescale.py" "${source_args[@]}" \
  --okgf-lib "$build/okgf/libokgf.so" --original-graphbuf "$build/graphbuf"

# The translated Pascal class hierarchy has RTTI references outside this
# fixture; retain ASan and UBSan checks while excluding only vptr checks.
san=(-O1 -g -fsanitize=address,undefined -fno-sanitize=vptr
     -fno-omit-frame-pointer)
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/GR_GraphBuf.cpp" \
  -o "$build/GR_GraphBuf_sanitized.o"
g++ "${common[@]}" "${san[@]}" -c "$root/upstream/cpp/src/GR_gi.cpp" \
  -o "$build/GR_gi_sanitized.o"
g++ "${common[@]}" "${san[@]}" "$root/tests/test_e2e5_original_graphbuf.cpp" \
  "$build/GR_GraphBuf_sanitized.o" "$build/GR_gi_sanitized.o" \
  "${link[@]}" -o "$build/graphbuf-sanitized"
python3 "$root/tools/e2e5_gamesettings_rescale.py" "${source_args[@]}" \
  --okgf-lib "$build/okgf/libokgf.so" --original-graphbuf "$build/graphbuf-sanitized"
