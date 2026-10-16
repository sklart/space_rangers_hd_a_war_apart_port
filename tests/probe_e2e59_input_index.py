"""Run deterministic gesture/provenance and bounded film-index checks on two CPUs."""
from pathlib import Path
import subprocess

out = Path('build/e2e59/input-index')
out.mkdir(parents=True, exist_ok=True)
common = ['-std=c++20', '-O2', '-Iport/switch/platform',
          'tests/test_e2e59_map_swipe.cpp', 'port/switch/platform/input_platform.cpp']
for compiler, name, runner in [('g++', 'host', []),
                              ('aarch64-linux-gnu-g++', 'arm64', ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu'])]:
    binary = out / name
    subprocess.run([compiler, *common, '-o', str(binary)], check=True)
    subprocess.run([*runner, str(binary)], check=True)
    print(f'{name}: gestures, touch provenance, exact ordinals, uncached exceptions PASS', flush=True)
