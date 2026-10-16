"""Exercise native heap fallback and worker/UI counters on host and ARM64."""
from pathlib import Path
import subprocess
import sys

root = Path.cwd()
sys.path.insert(0, str(root / 'tools'))
from e2e_runtime_overrides import prepare_runtime_overlay
out = root / 'build/e2e510/runtime-tests'
out.mkdir(parents=True, exist_ok=True)
overlay = prepare_runtime_overlay(root / 'upstream/cpp/runtime', out / 'overlay')
for compiler, name, runner, flags in [
    ('g++', 'host', [], ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']),
    ('aarch64-linux-gnu-g++', 'arm64', ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu'], [])]:
    for fixture in ['heap_reserve', 'generation_progress']:
        binary = out / (name + '-' + fixture)
        common = ['-std=c++20', '-O1', *flags, '-pthread', '-Iport/switch/platform',
            '-I' + str(overlay), '-Iupstream/cpp/runtime',
            'tests/test_e2e510_' + fixture + '.cpp']
        if fixture == 'heap_reserve':
            common += ['-Wl,--wrap=malloc,--wrap=realloc']
        subprocess.run([compiler, *common, '-o', str(binary)], check=True)
        subprocess.run([*runner, str(binary)], cwd=out, check=True)
