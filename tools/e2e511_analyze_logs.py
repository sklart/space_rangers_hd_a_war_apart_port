"""Read-only summary of hardware log evidence; never infers a hardware PASS."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import statistics
import zipfile


def summarize(path):
    with zipfile.ZipFile(path) as archive:
        log = archive.read('logs/e2e.log').decode('utf-8', errors='replace')
    builds = re.findall(r'build_git=(\S+)', log)
    # The file is append-only. Never combine timing from distinct binaries.
    log = '[E2E] build_git=' + log.rsplit('[E2E] build_git=', 1)[-1]
    turns = defaultdict(list)
    for operation, screen, elapsed in re.findall(
            r'turn operation=(\S+) screen=(\d+) elapsed_us=(\d+)', log):
        if screen != '3':
            turns[operation].append(int(elapsed))
    reserves = re.findall(r'heap-reserve bytes=(\d+) recoveries=(\d+)', log)
    phases = defaultdict(list)
    for phase, mean, maximum in re.findall(
            r'perf screen=21 source=arcade-phase stage=(\d+) count=\d+ avg_us=(\d+).*?max_us=(\d+)', log):
        phases[phase].append((int(mean), int(maximum)))
    return {
        'input_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'builds_in_archive': builds,
        'analyzed_build': builds[-1] if builds else None,
        'turns': {key: {'count': len(values), 'median_us': statistics.median(values),
                        'min_us': min(values), 'max_us': max(values)}
                  for key, values in turns.items()},
        'arcade_phases': {key: {'window_count': len(values),
                               'max_window_mean_us': max(x[0] for x in values),
                               'max_us': max(x[1] for x in values)}
                         for key, values in phases.items()},
        'reserve_last': {'bytes': int(reserves[-1][0]), 'recoveries': int(reserves[-1][1])}
                        if reserves else None,
        'last_input': re.findall(r'^.*input press_id=.*phase=dispatch.*$', log, re.M)[-1:],
        'tail': log.splitlines()[-6:],
        'interpretation': 'Observed evidence only. Hang cause and post-fix hardware behavior unverified.'
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('archive', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summarize(args.archive), indent=2) + '\n', encoding='utf-8')
