"""Exercise the exact disposable heap header used by the Switch E2E build."""

from pathlib import Path
import subprocess
import tempfile

from e2e_runtime_overrides import prepare_runtime_overlay


ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "upstream/cpp/runtime"

with tempfile.TemporaryDirectory(prefix="srhd-e2e-heap-") as directory:
    overlay = Path(directory)
    prepare_runtime_overlay(RUNTIME, overlay)
    executable = overlay / ("test_e2e_heaps.exe" if __import__("os").name == "nt" else "test_e2e_heaps")
    subprocess.run([
        "g++", "-std=c++20", "-O2", "-I" + str(overlay), "-I" + str(RUNTIME),
        str(ROOT / "tests/test_e2e_heaps.cpp"), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
