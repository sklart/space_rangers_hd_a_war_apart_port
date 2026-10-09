"""Exercise the Switch file-search adapter used by original FindFirst/FindNext."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "port/switch/build/e2e-file-search"
BUILD.mkdir(parents=True, exist_ok=True)
binary = BUILD / "test_e2e_file_search"
subprocess.run([
    "g++", "-std=gnu++20", "-O2", "-include", "unistd.h",
    "-ffunction-sections", "-fdata-sections", "-pthread",
    "-I" + str(ROOT / "port/switch/platform"),
    "-I" + str(ROOT / "upstream/cpp/src"),
    "-I" + str(ROOT / "upstream/cpp/runtime"),
    str(ROOT / "tests/test_e2e_file_search.cpp"),
    str(ROOT / "port/switch/platform/e2e_file_search.cpp"),
    str(ROOT / "port/switch/platform/e2e_file_match.cpp"),
    str(ROOT / "upstream/cpp/runtime/runtime.cpp"),
    "-Wl,--gc-sections", "-o", str(binary),
], cwd=ROOT, check=True)
subprocess.run([str(binary), str(BUILD / "fixture")], cwd=ROOT, check=True)
print("E2E file search PASS")
