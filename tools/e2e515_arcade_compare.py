"""Compare actual arena checkpoints, RNG and frame traces; no game state mutation."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import uuid

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    path = path.resolve(strict=True)
    if not path.is_relative_to(ROOT / 'build'):
        raise ValueError('Only isolated build reports are accepted')
    data = json.loads(path.read_text())
    rows = list(csv.reader((path.parent / 'arcade-events.csv').open()))
    images = {row[2]: row[3] for row in rows if row[0] == 'image-hash'}
    rng = {row[2]: [int(row[3]), int(row[4])] for row in rows if row[0] == 'rng'}
    sounds = [dict(tick=int(row[2]), rng_before=int(row[3]), time_ms=int(row[4]))
              for row in rows if row[0] == 'random-sound']
    checkpoints = [line for line in (path.parent / 'stderr.log').read_text(errors='replace').splitlines()
                   if line.startswith(('ARCADE_CHECKPOINT ', 'ARCADE_GEOMETRY '))]
    valid = (data['status'] == 'PASS' and data['returncode'] == 0 and any(row[0] == 'complete' for row in rows)
             and images == data.get('image_hashes_by_tick') and rng == data.get('rng_by_tick')
             and checkpoints == data['checkpoints'] and sounds == data.get('random_sound_events')
             and data.get('config_source_unchanged') is True)
    if data.get('execution_mode') == 'fixed timer/render replay':
        valid &= any(row[0] == 'replay-begin' for row in rows) and any(row[0] == 'replay-logical-ms' for row in rows)
    artifact = ROOT / 'build/e2e515/arcade/artifacts' / (data['binary_sha256'] + '.elf')
    valid &= artifact.is_file() and hashlib.sha256(artifact.read_bytes()).hexdigest() == data['binary_sha256']
    return data, valid

def differences(left, right):
    keys = sorted(set(left) | set(right), key=int)
    return [dict(tick=int(key), reference=left.get(key), other=right.get(key))
            for key in keys if left.get(key) != right.get(key)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('other', type=Path)
    args = parser.parse_args()
    a, valid_a = read(args.reference); b, valid_b = read(args.other)
    scenario_keys = ('seed', 'execution_mode', 'requested_ticks', 'death_tick', 'slow_ms')
    scenario = {key: [a.get(key), b.get(key)] for key in scenario_keys if a.get(key) != b.get(key)}
    state_a = sorted(set(line for line in a['checkpoints'] if line.startswith('ARCADE_CHECKPOINT ')))
    state_b = sorted(set(line for line in b['checkpoints'] if line.startswith('ARCADE_CHECKPOINT ')))
    geometry_a = sorted(set(line for line in a['checkpoints'] if line.startswith('ARCADE_GEOMETRY ')))
    geometry_b = sorted(set(line for line in b['checkpoints'] if line.startswith('ARCADE_GEOMETRY ')))
    rng = differences(a.get('rng_by_tick', {}), b.get('rng_by_tick', {}))
    image = differences(a['image_hashes_by_tick'], b['image_hashes_by_tick'])
    covered = bool(state_a and geometry_a and a.get('rng_by_tick') and b.get('rng_by_tick') and
                   a['image_hashes_by_tick'] and b['image_hashes_by_tick'])
    report = dict(status='PARITY_PASS' if valid_a and valid_b and covered and not scenario and
                  state_a == state_b and geometry_a == geometry_b and not rng and not image else 'PARITY_FAIL',
                  reference=str(args.reference), other=str(args.other),
                  raw_csv_and_stderr_verified=bool(valid_a and valid_b), required_traces_present=covered,
                  scenario_diff=scenario, state_equal=state_a == state_b, geometry_equal=geometry_a == geometry_b,
                  rng_recorded_ticks=[len(a.get('rng_by_tick', {})), len(b.get('rng_by_tick', {}))],
                  rng_differences=len(rng), first_rng_difference=rng[0] if rng else None,
                  image_recorded_ticks=[len(a['image_hashes_by_tick']), len(b['image_hashes_by_tick'])],
                  image_differences=len(image), first_image_difference=image[0] if image else None,
                  random_sound_events_equal=a.get('random_sound_events') == b.get('random_sound_events'),
                  random_sound_event_counts=[len(a.get('random_sound_events', [])), len(b.get('random_sound_events', []))],
                  main_thread_cpu_ns=[a['total_thread_cpu_ns'], b['total_thread_cpu_ns']],
                  observed_cpu_change_percent=100 * (b['total_thread_cpu_ns'] / a['total_thread_cpu_ns'] - 1),
                  three_paired_series_complete=False, hardware_executed=False,
                  parity_scope='recorded object scalars, RNG, geometry and software frames; not full world serialization or input latency')
    output = ROOT / 'build/e2e515/arcade/comparisons' / (uuid.uuid4().hex + '.json')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + '\n')
    report['comparison_report'] = str(output)
    print(json.dumps(report, indent=2), flush=True)
    if report['status'] != 'PARITY_PASS': raise SystemExit(1)

if __name__ == '__main__': main()
