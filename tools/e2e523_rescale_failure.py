#!/usr/bin/env python3
"""Targeted original GraphBuf failure/retry check (Linux/WSL, no game assets).

--library-build selects an already built OKGF. Its CMakeCache determines the
math flags. --rescale-source may select the frozen prepared fast24 source.
The original generated Ex_OKGF_Rescale body is compiled, not a matching mock.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from e2e_source_overrides import generated_source


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library-build", type=Path, required=True)
    parser.add_argument("--rescale-source", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gi", type=Path, help="Optional private extracted real GI; otherwise use synthetic Format0")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    library = args.library_build.resolve()
    rescale = (args.rescale_source or root / "upstream/okgf/src/rescale.c").resolve()
    okgf = rescale.parent.parent
    out = args.output.resolve()
    # Output is intentionally confined to private build files in this checkout.
    if not out.is_relative_to(root / "build"):
        parser.error("--output must be inside this checkout's build directory")
    out.mkdir(parents=True, exist_ok=True)
    cache = (library / "CMakeCache.txt").read_text()
    native = "OKGF_MATH_BACKEND:STRING=NATIVE" in cache
    compatible = "OKGF_MATH_BACKEND:STRING=COMPATIBLE" in cache
    if not (native or compatible):
        parser.error("this check requires a NATIVE or COMPATIBLE OKGF build")
    generated_source(root / "upstream/cpp/src/GR_GraphBuf.cpp", out / "GR_GraphBuf.cpp")
    generated_source(root / "upstream/cpp/src/GR_Main.cpp", out / "GR_Main.cpp")
    generated_source(root / "upstream/cpp/src/EC_CacheGI.cpp", out / "EC_CacheGI.cpp")
    source = (out / "GR_Main.cpp").read_text()
    start = source.index("    void Ex_OKGF_Rescale(")
    end = source.index("\n    void Ex_OKGR_F5_DrawRGBA", start)
    (out / "wrapper.cpp").write_text(
        '#include "units/GR_Main.hpp"\n#include "e2e5_checked_rescale.hpp"\n'
        '#include "e2e_stage.hpp"\nnamespace GR_Main {\n' + source[start:end] + "\n}\n")
    sanitizer = ["-fsanitize=address,undefined", "-fno-sanitize=vptr", "-fno-omit-frame-pointer"]
    common = ["g++", "-std=gnu++20", "-O1", "-g", "-D__SWITCH__", "-DE2E5_ORIGINAL_CACHEGI", "-ffunction-sections",
              "-fdata-sections", "-Wno-invalid offsetof".replace(" ", "-"), *sanitizer]
    for path in ["port/switch/platform", "upstream/cpp/src", "upstream/cpp/runtime", "upstream/okgf/include"]:
        common.append("-I" + str(root / path))
    cflags = ["gcc", "-std=c11", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
              "-fno-fast-math", "-ffp-contract=off", "-DOKGF_GAME_RELEASE=OKGF_GAME_SRHD",
              "-Dmalloc=rescale_test_malloc", "-Dcalloc=rescale_test_calloc",
              "-I" + str(okgf / "include"), "-I" + str(okgf / "src")]
    if native:
        cflags.append("-DOKGF_NATIVE_MATH=1")
    else:
        cflags += ["-DSOFTFLOAT_FAST_INT64", "-I" + str(okgf / "vendor/softfloat"),
                   "-I" + str(okgf / "vendor/softfloat/source/include")]
    if "OKGF_PORTABLE_MATH:BOOL=ON" in cache:
        cflags.append("-DOKGF_PORTABLE_MATH=1")
    if "-D_Thread_local=" in cache:
        cflags.append("-D_Thread_local=")
    commands = [cflags + ["-c", str(rescale), "-o", str(out / "rescale.o")]]
    objects = [out / "rescale.o"]
    units = {"GR_GraphBuf": out / "GR_GraphBuf.cpp", "wrapper": out / "wrapper.cpp",
             "EC_CacheGI": out / "EC_CacheGI.cpp",
             **{name: root / ("upstream/cpp/src/" + name + ".cpp")
                for name in ["GR_gi", "EC_Buf", "EC_Struct", "EC_Cache", "EC_Str"]},
             "runtime": root / "upstream/cpp/runtime/runtime.cpp"}
    for name, path in units.items():
        obj = out / (name + ".o")
        commands.append(common + ["-c", str(path), "-o", str(obj)])
        objects.append(obj)
    if compatible:
        # The backend's internal SoftFloat helpers have hidden visibility in
        # the DSO. Link the already-built matching objects/archive explicitly
        # for the executable's allocator-instrumented rescale translation unit.
        objects += [library / "CMakeFiles/okgf.dir/src/math/trig_x87.c.o",
                    library / "CMakeFiles/okgf.dir/src/math/trig_emulated.c.o",
                    library / "vendor/softfloat/libokgf_softfloat.a"]
    commands.append(common + [str(root / "tests/test_e2e523_rescale_transaction.cpp"),
                    *map(str, objects), "-Wl,--gc-sections", "-L" + str(library), "-lokgf",
                    "-Wl,-rpath," + str(library), "-o", str(out / "test")])
    build = []
    for command in commands:
        run = subprocess.run(command, text=True, capture_output=True)
        build.append(dict(command=command, exit=run.returncode, stdout=run.stdout, stderr=run.stderr))
        (out / "build.json").write_text(json.dumps(build, indent=2))
        if run.returncode:
            print(run.stderr)
            return 1
    run = subprocess.run([str(out / "test")] + ([str(args.gi.resolve())] if args.gi else []), text=True, capture_output=True,
                         env=os.environ | {"ASAN_OPTIONS": "detect_leaks=1"})
    report = dict(status="HOST_PASS" if not run.returncode else "FAIL",
                  backend="NATIVE" if native else "COMPATIBLE", source=str(rescale),
                  rescale_sha256=hashlib.sha256(rescale.read_bytes()).hexdigest(),
                  library_sha256=hashlib.sha256((library / "libokgf.so").read_bytes()).hexdigest(),
                  exit=run.returncode, stdout=run.stdout, stderr=run.stderr,
                  hardware=False, arm64=False,
                  private_gi=str(args.gi.resolve()) if args.gi else None,
                  boundary="synthetic GraphBuf; original wrapper/GraphBuf/CacheGI, allocation failures, retry, ASan/UBSan/leaks; no concurrent global-cache AcquireDataFromConfig or hardware proof")
    (out / "result.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))
    return int(run.returncode != 0)


if __name__ == "__main__":
    raise SystemExit(main())
