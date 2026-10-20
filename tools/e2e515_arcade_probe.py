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

def build(variant):
    variant_root = OUTPUT / 'variants' / variant
    variant_root.mkdir(parents=True, exist_ok=True)
    (variant_root / 'build-status.json').write_text(json.dumps({'status': 'RUNNING'}))
    report = json.loads((BUILD / 'report.json').read_text())
    mapping = report['source_map']
    flags = report['compile_flags']
    flags = [*flags, '-I' + str(ROOT / 'tests')]
    header = ROOT / 'tests/e2e515_arcade_observer.hpp'
    sources = {}
    for name in ('ab_MainForm', 'ab_Hit', 'ab_Global', 'ab_Object', 'ab_ShipAI', 'ab_Polygon', 'GI_MessageLoop', 'SE_Space', 'MMSystem'):
        sources[name] = ROOT / mapping[f'upstream/cpp/src/{name}.cpp']
    sources['renderer_platform'] = ROOT / 'port/switch/platform/renderer_platform.cpp'
    objects = [Path(p) for p in (BUILD / 'link-objects.rsp').read_text().splitlines()
               if Path(p).stem not in sources]
    provenance = {'host_binary_sha256': report['binary_sha256'], 'header_sha256': sha(header),
                  'clock_header_sha256': sha(ROOT / 'tests/e2e515_arcade_clock.hpp'),
                  'memo_header_sha256': sha(ROOT / 'tests/e2e515_polygon_projection.hpp'), 'variant': variant}
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
                replace('        LoadPanel->OnOpen();', '        LoadPanel->OnOpen();\n        e2e515_arcade_observer::BeginReplayClock(this);\n        System::RandSeed = 0x515u;')
                for stage, scope in ((47, 'Tick tick(this)'), (46, 'Draw draw')):
                    marker = f'srhd_awa::platform::e2e5_stage_profiler::Scope e2e_ab_scope(srhd_awa::platform::e2e5_stage_profiler::detail_window, {stage});'
                    replace(marker, marker + '\n        e2e515_arcade_observer::' + scope + ';')
            elif name == 'ab_Hit':
                marker = '    void TabHit::ExplosionComplete(GI_MessageLoop::TObjectGI* Sender) {'
                replace(marker, marker + '\n        e2e515_arcade_observer::ExplosionComplete(this);')
            elif name == 'renderer_platform':
                replace('  e2e5_stage_profiler::PresentFrame(width, height);',
                        '  e2e5_stage_profiler::PresentFrame(width, height);\n  e2e515_arcade_observer::Present();')
            elif name == 'GI_MessageLoop':
                anchor = '            e2e_progress.Mark(11);\n            if (ExitCode == 0) {\n                DrawFrame();'
                replace(anchor, '            e2e_progress.Mark(11);\n            e2e515_arcade_observer::Replay(this);\n            if (ExitCode == 0) {\n                DrawFrame();')
            elif name == 'MMSystem':
                anchor = '    std::uint32_t PAS_STDCALL timeGetTime() {'
                replace(anchor, anchor + '\n        if (e2e515_arcade_clock::enabled) return e2e515_arcade_clock::milliseconds;')
            elif name == 'SE_Space':
                anchor = '                        if (NextSoundTime < Now) {'
                replace(anchor, anchor + '\n                            e2e515_arcade_observer::Row("random-sound", System::RandSeed, Now);')
            if name == 'ab_Polygon' and variant == 'polygon-memo':
                from e2e515_polygon_overrides import apply
                text = apply(text)
            scopes = {'ab_Global': (('ComputeSphericalBearingAndDistance', 1), ('ProjectPointByMatrix', 7)),
                      'ab_Object': (('TabObject::DistanceTo', 2), ('TabObject::Advance', 3)),
                      'ab_ShipAI': (('TabShipAI::Advance', 4), ('TabShipAI::DecideActions', 5),
                                    ('TabShipAI::AvoidImmediateObstacle', 6))}
            for function, number in scopes.get(name, ()):
                markers = [line for line in text.splitlines() if line.startswith('    ') and
                           not line.startswith('     ') and function + '(' in line and line.endswith(' {')]
                if len(markers) != 1: raise RuntimeError('Hotspot boundary changed: ' + function)
                replace(markers[0], markers[0] + f'\n        e2e515_arcade_observer::Scope e2e_hotspot({number});')
            text = f'#include "{header}"\n' + text
            generated = temp / (name + '.cpp'); generated.write_text(text)
            cache = OUTPUT / 'objects'; cache.mkdir(exist_ok=True)
            memo_header = ROOT / 'tests/e2e515_polygon_projection.hpp'
            key = hashlib.sha256((text + sha(header) + sha(ROOT / 'tests/e2e515_arcade_clock.hpp') + sha(memo_header) + repr(flags) + report['binary_sha256']).encode()).hexdigest()
            obj = cache / (name + '-' + key + '.o')
            if not obj.exists():
                temporary_obj = temp / (name + '.o')
                subprocess.run(['g++', *flags, '-c', str(generated), '-o', str(temporary_obj)], cwd=ROOT, check=True)
                pending_obj = obj.with_suffix('.' + uuid.uuid4().hex + '.tmp')
                shutil.copyfile(temporary_obj, pending_obj)
                pending_obj.replace(obj)
            print(name + ': diagnostic object ready', flush=True)
            staged.append(str(obj))
            provenance[name] = {'source_sha256': sha(source), 'generated_sha256': sha(generated)}
        rsp = temp / 'objects.rsp'; rsp.write_text('\n'.join(staged) + '\n')
        binary = variant_root / 'arena-host'
        linked = temp / 'arena-host'
        library = BUILD / 'okgf/libokgf.so'
        subprocess.run(['g++', '@' + str(rsp), '-Wl,--gc-sections', str(library),
                        '-Wl,-rpath,' + str(library.parent), '-lz', '-lpng', '-ljpeg',
                        '-lvorbisfile', '-lvorbis', '-logg', '-lSDL2', '-lm', '-pthread',
                        '-o', str(linked)], cwd=ROOT, check=True)
        pending_binary = binary.with_suffix('.' + uuid.uuid4().hex + '.tmp')
        shutil.copyfile(linked, pending_binary); pending_binary.chmod(0o755); pending_binary.replace(binary)
        provenance['binary_sha256'] = sha(binary)
        artifacts = OUTPUT / 'artifacts'; artifacts.mkdir(exist_ok=True)
        preserved = artifacts / (provenance['binary_sha256'] + '.elf')
        if not preserved.exists(): shutil.copyfile(binary, preserved)
        (variant_root / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
        (variant_root / 'build-status.json').write_text(json.dumps({'status': 'PASS', 'binary_sha256': sha(binary)}))
        print('Diagnostic arena linked', flush=True)

def percentile(values, fraction):
    values = sorted(values)
    return values[min(len(values) - 1, int((len(values) - 1) * fraction))] if values else None

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--variant', choices=('baseline', 'polygon-memo'), default='baseline',
                        help='polygon-memo is a rejected diagnostic comparison, never a shipping feature')
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--config-root', type=Path, required=True)
    parser.add_argument('--slow-ms', type=int, choices=(0, 20, 40, 70, 150, 250), default=0)
    parser.add_argument('--death-tick', type=int, default=0)
    parser.add_argument('--ticks', type=int, choices=(100, 500, 1000), default=500)
    parser.add_argument('--fixed-replay', action='store_true', help='Controlled UI-thread clock and original timer queue, one draw per 20 logical ms; not a live scheduler/FPS test')
    args = parser.parse_args()
    if not 0 <= args.death_tick <= 1000: parser.error('death tick outside 0..1000')
    if args.fixed_replay and args.slow_ms: parser.error('Slow callback starvation tests require the live loop')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    variant_root = OUTPUT / 'variants' / args.variant
    if args.build:
        try: build(args.variant)
        except Exception as error:
            (variant_root / 'build-status.json').write_text(json.dumps({'status': 'FAIL', 'error': str(error)}))
            raise
    binary = variant_root / 'arena-host'
    provenance = json.loads((variant_root / 'provenance.json').read_text())
    if provenance['binary_sha256'] != sha(binary): raise RuntimeError('Probe binary changed')
    build_status = variant_root / 'build-status.json'
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
    env['E2E515_ARCADE_REPLAY'] = str(int(args.fixed_replay))
    report = dict(status='RUNNING', stage=str(stage), variant=args.variant, slow_ms=args.slow_ms, death_tick=args.death_tick,
                  requested_ticks=args.ticks, binary_sha256=sha(binary), hardware_executed=False,
                  presentation_backend='host software PresentRgb565 completion; no SDL GPU presentation', seed=0x515,
                  checkpoint_scope='selected object scalar fields, health, form/System RNG; not full world serialization',
                  execution_mode='fixed timer/render replay' if args.fixed_replay else 'live loop',
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
                             if line.startswith(('ARCADE_CHECKPOINT ', 'ARCADE_GEOMETRY '))]
    hotspot_path = stage / 'arcade-hotspots.csv'
    report['sampled_inclusive_hotspots'] = {row[0]: dict(calls=int(row[1]), samples=int(row[2]), sampled_cpu_ns=int(row[3]), max_sample_cpu_ns=int(row[4]))
                                    for row in csv.reader(hotspot_path.open())} if hotspot_path.exists() else {}
    report['image_hashes_by_tick'] = {row[2]: row[3] for row in rows if row[0] == 'image-hash'}
    report['rng_by_tick'] = {row[2]: [int(row[3]), int(row[4])] for row in rows if row[0] == 'rng'}
    report['random_sound_events'] = [dict(tick=int(row[2]), rng_before=int(row[3]), time_ms=int(row[4]))
                                      for row in rows if row[0] == 'random-sound']
    if args.death_tick and not (report['death_observed'] and report['explosion_complete'] and report['result_presented']):
        report['status'] = 'FAIL'
    if not report['config_source_unchanged']: report['status'] = 'FAIL'
    report['phase_events'] = [{'event': row[0], 'wall_ns': int(row[1]), 'tick': int(row[2])}
                              for row in rows if row[0] not in ('tick', 'draw', 'present', 'image-hash', 'rng', 'random-sound')]
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
    display = dict(report)
    display['image_hashes_by_tick'] = {'recorded_ticks': len(report['image_hashes_by_tick'])}
    display['rng_by_tick'] = {'recorded_ticks': len(report['rng_by_tick'])}
    print(json.dumps(display, indent=2), flush=True)
    if report['status'] != 'PASS': raise SystemExit(1)

if __name__ == '__main__': main()
