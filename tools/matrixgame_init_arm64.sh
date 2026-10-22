#!/usr/bin/env bash
# New integrated cache/3G CPU contract only. No replay of old component suites.
set -euo pipefail
cd "$(dirname "$0")/.."
root="$PWD"
out="${1:?fresh build output required}";shift
[[ "$out" == build/* && ! -e "$out" && "$out" != *..* ]] || { echo 'Use a fresh build/ output' >&2;exit 2; }
png="$root/build/matrixgame/e2e518/deps/libpng"
jpeg="$root/build/matrixgame/e2e518/deps/jpeg"
zsrc="$root/build/e2e515/deps/zlib-1.3.2"
zlib="$root/build/e2e515/mg-navigation-arm64/zlib/libz.a"
matrix_git='/mnt/c/Program Files/Git/cmd/git.exe'
for pair in "$png:a40189cf881e9f0db80511c382292a5604c3c3d1" "$jpeg:ecf021bc0d6f435daacff7c35ccaeef0145df1b9" "$zsrc:da607da739fa6047df13e66a2af6b8bec7c2a498";do
    source="${pair%:*}";pin="${pair##*:}"
    [[ "$("$matrix_git" -C "$(wslpath -w "$source")" rev-parse HEAD | tr -d '\r')" == "$pin" ]]
    [[ -z "$("$matrix_git" -C "$(wslpath -w "$source")" status --porcelain)" ]]
done
mkdir -p "$out"
# Export verified Git blobs rather than the Windows checkout's CRLF scripts:
# libpng's options.awk cannot parse CRLF pnglibconf.dfa on Linux.
mkdir "$out/png-source" "$out/jpeg-source"
"$matrix_git" -C "$(wslpath -w "$png")" archive HEAD | tar -x -C "$out/png-source"
"$matrix_git" -C "$(wslpath -w "$jpeg")" archive HEAD | tar -x -C "$out/jpeg-source"
python3 - "$out" <<'PY'
import pathlib,sys,json,hashlib
root=pathlib.Path(sys.argv[1]);changed=[]
for tree in ['png-source','jpeg-source']:
    for path in (root/tree).rglob('*'):
        if path.is_file() and path.suffix.lower() in {'.c','.h','.txt','.cmake','.awk','.dfa','.in','.prebuilt','.map','.def','.ac','.am','.sh'}:
            data=path.read_bytes();normal=data.replace(b'\r\n',b'\n')
            if normal!=data:
                changed.append(dict(path=str(path.relative_to(root)),before=hashlib.sha256(data).hexdigest(),after=hashlib.sha256(normal).hexdigest()))
                path.write_bytes(normal)
(root/'source-normalization.json').write_text(json.dumps(changed,indent=2)+'\n')
PY
png="$root/$out/png-source";jpeg="$root/$out/jpeg-source"
cmake -S "$png" -B "$out/png" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_BUILD_TYPE=Release -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF \
    -DM_LIBRARY="$(aarch64-linux-gnu-gcc -print-file-name=libm.a)" \
    -DZLIB_INCLUDE_DIR="$zsrc" -DZLIB_LIBRARY="$zlib" > "$out/png-configure.log" 2>&1
cmake --build "$out/png" --target png_static --parallel 4 > "$out/png-build.log" 2>&1
cmake -S "$jpeg" -B "$out/jpeg" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF \
    > "$out/jpeg-configure.log" 2>&1
cmake --build "$out/jpeg" --target jpeg-static --parallel 4 > "$out/jpeg-build.log" 2>&1
cmake -S port/matrixgame/native-engine -B "$out/engine" \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++-15 \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DZLIB_INCLUDE_DIR="$zsrc" -DZLIB_LIBRARY="$zlib" \
    -DPNG_PNG_INCLUDE_DIR="$png" -DPNG_LIBRARY_RELEASE="$root/$out/png/libpng16.a" \
    -DJPEG_INCLUDE_DIR="$jpeg" -DJPEG_LIBRARY_RELEASE="$root/$out/jpeg/libjpeg.a" \
    -DCMAKE_CXX_FLAGS="-pthread -I$root/$out/png -I$root/$out/jpeg" \
    > "$out/engine-configure.log" 2>&1
cmake --build "$out/engine" --target matrixgame-native-init-probe --parallel 4 > "$out/build.log" 2>&1
python3 tests/probe_matrixgame_native_init.py --binary "$out/engine/matrixgame-native-init-probe" --qemu "$@" --output "$out/runtime"
