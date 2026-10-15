"""Run the host timing regression while keeping the upstream submodule clean."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "port/switch/build/e2e-time"
RUNTIME = ROOT / "upstream/cpp/runtime"
SRC = ROOT / "upstream/cpp/src"
PLATFORM = ROOT / "port/switch/platform"


def main() -> int:
    BUILD.mkdir(parents=True, exist_ok=True)
    compiler = os.getenv("CXX", "g++")
    target = subprocess.check_output([compiler, "-dumpmachine"], text=True).strip()
    includes = [PLATFORM, SRC, RUNTIME]
    if "cygwin" in target or "msys" in target:
        original = (RUNTIME / "text.hpp").read_text(encoding="utf-8")
        old = "#else\n              if (::ftruncate(::fileno(f.stream), end))"
        new = ("#elif defined(__MSYS__)\n"
               "              std::error_code error;\n"
               "              std::filesystem::resize_file(f.name, static_cast<uintmax_t>(end), error);\n"
               "              if (error)\n"
               "                in_out_res = file_error();\n"
               "#else\n              if (::ftruncate(::fileno(f.stream), end))")
        if old not in original:
            raise RuntimeError("MSYS text overlay context changed")
        (BUILD / "text.hpp").write_text(original.replace(old, new), encoding="utf-8")
        includes.insert(0, BUILD)
    exe = BUILD / ("test_e2e_time.exe" if os.name == "nt" else "test_e2e_time")
    command = [compiler, "-std=c++20", "-O2",
               *["-I" + str(path) for path in includes],
               str(ROOT / "tests/test_e2e_time.cpp"),
               str(PLATFORM / "runtime_time.cpp"), "-o", str(exe)]
    subprocess.run(command, cwd=ROOT, check=True)
    return subprocess.run([str(exe)], cwd=ROOT).returncode


if __name__ == "__main__":
    raise SystemExit(main())
