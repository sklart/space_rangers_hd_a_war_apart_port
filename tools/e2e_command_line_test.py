"""Exercise the original command-line parser with Switch's generated imports."""

from pathlib import Path
import subprocess

from e2e_source_overrides import generated_source
from e2e_runtime_overrides import prepare_runtime_overlay
from e2e_win32_host_test import implementation_sources


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "port/switch/build/e2e-command-line"
BUILD.mkdir(parents=True, exist_ok=True)
OVERLAY = prepare_runtime_overlay(ROOT / "upstream/cpp/runtime", BUILD / "runtime-overlay")

imports = []
for name in ("WindowsImports.cpp", "WindowsSdk.cpp"):
    imports.append(generated_source(ROOT / "upstream/cpp/src" / name, BUILD / name))

binary = BUILD / "test_e2e_command_line"
subprocess.run([
    "g++", "-std=gnu++20", "-O2", "-include", "unistd.h",
    "-DE2E_HOST_RESOLVER_TEST", "-DE2E_HOST_OKGF_STUB",
    "-ffunction-sections", "-fdata-sections", "-pthread",
    "-I" + str(OVERLAY),
    "-I" + str(ROOT / "port/switch/platform"),
    "-I" + str(ROOT / "upstream/cpp/src"),
    "-I" + str(ROOT / "upstream/cpp/runtime"),
    str(ROOT / "tests/test_e2e_command_line.cpp"),
    str(ROOT / "upstream/cpp/src/SystemImports.cpp"),
    str(ROOT / "upstream/cpp/src/System.cpp"),
    *(str(path) for path in imports),
    *implementation_sources(),
    str(ROOT / "upstream/cpp/runtime/runtime.cpp"),
    "-Wl,--gc-sections", "-lz", "-o", str(binary),
], cwd=ROOT, check=True)
subprocess.run([str(binary)], cwd=ROOT, check=True)
print("E2E command line PASS")
