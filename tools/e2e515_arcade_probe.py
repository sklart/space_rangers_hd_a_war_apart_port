"""Build and run instrumented real host arena; never modifies upstream or NRO."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'port/switch/build/e2e5-host-game/optimized'
OUTPUT = ROOT / 'build/e2e515/arcade'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def build():
    (OUTPUT / 'build-status.json').write_text(json.dumps({'status': 'RUNNING'}))
    report = json.loads((BUILD / 'report.json').read_text())
    mapping = report['source_map']
    flags = report['compile_flags']
    header = ROOT / 'tests/e2e515_arcade_observer.hpp'
    sources = {}
    for name in ('ab_MainForm', 'ab_Hit'):
        sources[name] = ROOT / mapping[f'upstream/cpp/src/{name}.cpp']
    sources['renderer_platform'] = ROOT / 'port/switch/platform/renderer_platform.cpp'
    objects = [Path(p) for p in (BUILD / 'link-objects.rsp').read_text().splitlines()
               if Path(p).stem not in sources]
    provenance = {'host_binary_sha256': report['binary_sha256'], 'header_sha256': sha(header)}
    with tempfile.TemporaryDirectory(prefix='e2e515-arena-') as tmp:
        temp = Path(tmp)
        staged = []
        for i, obj in enumerate(objects):
            target = temp / f'base-{i}.o'; shutil.copyfile(obj, target); staged.append(str(target))
        for name, source in sources.items():
            text = source.read_text()
            def replace(old, new):
                nonlocal text
                if text.count(old) != 1:
                    raise RuntimeError('Unexpected probe boundary: ' + old)
                text = text.replace(old, new)
            if name == 'ab_MainForm':
                replace('        LoadPanel->OnOpen();', '        LoadPanel->OnOpen();\n        System::RandSeed = 0x515u;')
                for stage, scope in ((47, 'Tick tick(this)'), (46, 'Draw draw')):
                    marker = f'srhd_awa::platform::e2e5_stage_profiler::Scope e2e_ab_scope(srhd_awa::platform::e2e5_stage_profiler::detail_window, {stage});'
                    replace(marker, marker + '\n        e2e515_arcade_observer::' + scope + ';')
            elif name == 'ab_Hit':
                marker = '    void TabHit::ExplosionComplete(GI_MessageLoop::TObjectGI* Sender) {'
                replace(marker, marker + '\n        e2e515_arcade_observer::ExplosionComplete(this);')
            else:
                replace('  e2e5_stage_profiler::PresentFrame(width, height);',
                        '  e2e5_stage_profiler::PresentFrame(width, height);\n  e2e515_arcade_observer::Present();')
            text = f'#include "{header}"\n' + text
            generated = temp / (name + '.cpp'); generated.write_text(text)
            obj = temp / (name + '.o')
            subprocess.run(['g++', *flags, '-c', str(generated), '-o', str(obj)], cwd=ROOT, check=True)
            staged.append(str(obj))
            provenance[name] = {'source_sha256': sha(source), 'generated_sha256': sha(generated)}
        rsp = temp / 'objects.rsp'; rsp.write_text('\n'.join(staged) + '\n')
        binary = OUTPUT / 'arena-host'
        library = BUILD / 'okgf/libokgf.so'
        subprocess.run(['g++', '@' + str(rsp), '-Wl,--gc-sections', str(library),
                        '-Wl,-rpath,' + str(library.parent), '-lz', '-lpng', '-ljpeg',
                        '-lvorbisfile', '-lvorbis', '-logg', '-lSDL2', '-lm', '-pthread',
                        '-o', str(binary)], cwd=ROOT, check=True)
        provenance['binary_sha256'] = sha(binary)
        (OUTPUT / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        (OUTPUT / 'build-status.json').write_text(json.dumps({'status': 'PASS', 'binary_sha256': sha(binary)}))
        print('Diagnostic arena linked', flush=True)

def percentile(values, fraction):
    values = sorted(values)
    return values[min(len(values) - 1, int((len(values) - 1) * fraction))] if values else None

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--config-root', type=Path, required=True)
    parser.add_argument('--slow-ms', type=int, choices=(0, 20, 40, 70, 150, 250), default=0)
    parser.add_argument('--death-tick', type=int, default=0)
    parser.add_argument('--ticks', type=int, choices=(100, 500, 1000), default=500)
    args = parser.parse_args()
    if not 0 <= args.death_tick <= 1000: parser.error('death tick outside 0..1000')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    if args.build: build()
    binary = OUTPUT / 'arena-host'
    provenance = json.loads((OUTPUT / 'provenance.json').read_text())
    if provenance['binary_sha256'] != sha(binary): raise RuntimeError('Probe binary changed')
    build_status = OUTPUT / 'build-status.json'
    if build_status.exists() and json.loads(build_status.read_text()).get('status') != 'PASS':
        raise RuntimeError('Last diagnostic build did not finish successfully')
    stage = OUTPUT / 'runs' / uuid.uuid4().hex
    config = stage / 'user/config'; config.mkdir(parents=True)
    config_source = args.config_root / 'CFG.TXT'
    config_hash = sha(config_source)
    shutil.copyfile(config_source, config / 'CFG.TXT')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('E2E5_HOST_', 'E2E511_HOST_', 'E2E513_HOST_', 'E2E515_HOST_'))}
    env.update(E2E5_GAME_ROOT=str(args.game_root.resolve()), E2E5_HOST_MENU_PROBE='A',
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', LD_LIBRARY_PATH=str(BUILD / 'okgf'),
               E2E515_ARCADE_SLOW_MS=str(args.slow_ms), E2E515_ARCADE_DEATH_TICK=str(args.death_tick),
               E2E515_ARCADE_TICKS=str(args.ticks))
    report = dict(status='RUNNING', stage=str(stage), slow_ms=args.slow_ms, death_tick=args.death_tick,
                  requested_ticks=args.ticks, binary_sha256=sha(binary), hardware_executed=False,
                  presentation_backend='host software PresentRgb565 completion; no SDL GPU presentation', seed=0x515,
                  checkpoint_scope='selected object scalar fields, health, form/System RNG; not full world serialization',
                  normal_shutdown_verified=False)
    result = stage / 'result.json'; result.write_text(json.dumps(report, indent=2))
    print('Arena stage: ' + str(stage), flush=True)
    with (stage / 'stdout.log').open('wb') as out, (stage / 'stderr.log').open('wb') as err:
        process = subprocess.Popen([str(binary)], cwd=stage, env=env, stdout=out, stderr=err)
        try:
            code = process.wait(timeout=420)
        except subprocess.TimeoutExpired:
            process.terminate()
            try: process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=15)
            code = -1
    path = stage / 'arcade-events.csv'
    rows = list(csv.reader(path.open())) if path.exists() else []
    events = [row[0] for row in rows]
    presents = [int(row[1]) for row in rows if row[0] == 'present']
    gaps = [b - a for a, b in zip(presents, presents[1:])]
    report.update(status='PASS' if code == 0 and 'complete' in events else 'FAIL', returncode=code,
                  config_source_unchanged=sha(config_source) == config_hash, presents=len(presents),
                  tick_calls=events.count('tick'), death_observed='death' in events,
                  explosion_complete='explosion-complete' in events, result_presented='result-presented' in events,
                  present_gap_ns={name: percentile(gaps, fraction) for name, fraction in
                                  (('p50', .5), ('p95', .95), ('p99', .99), ('max', 1))})
    # A completed result alone must not hide the original tens-of-seconds stall.
    # Allow cold resource work, but fail any multi-second starvation interval.
    report['liveness_limit_ns'] = 2000000000 + args.slow_ms * 1000000
    if not gaps or max(gaps) > report['liveness_limit_ns'] or 'exception' in events:
        report['status'] = 'FAIL'
    report['checkpoints'] = [line for line in (stage / 'stderr.log').read_text(errors='replace').splitlines()
                             if line.startswith('ARCADE_CHECKPOINT ')]
    if args.death_tick and not (report['death_observed'] and report['explosion_complete'] and report['result_presented']):
        report['status'] = 'FAIL'
    if not report['config_source_unchanged']: report['status'] = 'FAIL'
    report['phase_events'] = [{'event': row[0], 'wall_ns': int(row[1]), 'tick': int(row[2])}
                              for row in rows if row[0] not in ('tick', 'draw', 'present')]
    for event, column, name in (('tick', 3, 'tick_wall_ns'), ('tick', 4, 'tick_cpu_ns'),
                                ('draw', 4, 'draw_cpu_ns')):
        values = [int(row[column]) for row in rows if row[0] == event]
        report[name] = {key: percentile(values, fraction) for key, fraction in
                        (('p50', .5), ('p95', .95), ('p99', .99), ('max', 1))}
        report[name]['total'] = sum(values)
    complete = [row for row in rows if row[0] == 'complete']
    if complete:
        report['total_wall_ns'] = int(complete[-1][3])
        report['total_thread_cpu_ns'] = int(complete[-1][4])
    result.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
    if report['status'] != 'PASS': raise SystemExit(1)

if __name__ == '__main__': main()
