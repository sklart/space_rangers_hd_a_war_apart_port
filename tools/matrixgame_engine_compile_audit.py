"""Compile every selected real CMake engine unit; archive/link/runtime are separate gates."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--target', action='append')
    p.add_argument('--jobs', type=int, default=2, choices=(1, 2, 3, 4))
    a = p.parse_args()
    build = a.build_root.resolve(strict=True); out = a.output.resolve()
    if not build.is_relative_to(ROOT / 'build') or not out.is_relative_to(ROOT / 'build'):
        raise ValueError('Use isolated build directories')
    if out.exists() and any(out.iterdir()):
        raise ValueError('Audit output must be fresh; preserve previous evidence')
    out.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / 'compile_commands.json').read_text())
    targets = a.target or ['matrixgame-native-engine']
    commands = [c for c in commands if any('/' + t + '.dir/' in c['command'] for t in targets)]
    if not commands: raise ValueError('No matching engine compilation commands')
    report = dict(status='RUNNING', scope='actual objects only; no full link/runtime claim', units=[], targets=targets)
    destination = out / 'result.json'
    def save(): destination.write_text(json.dumps(report, indent=2) + '\n')
    save()
    def compile_one(index, command):
        args = shlex.split(command['command'])
        obj = out / f'{index}.o'
        args[args.index('-o') + 1] = str(obj)
        source = Path(command['file'])
        relative = source.as_posix().split('/source/', 1)[-1]
        run = subprocess.run(args, cwd=command['directory'], stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, timeout=240)
        (out / f'{index}.log').write_text(run.stdout)
        error = next((line for line in run.stdout.splitlines() if re.search(r'(fatal )?error:', line)), '')
        row = dict(source=relative, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                   status='COMPILE_PASS' if run.returncode == 0 else 'COMPILE_FAIL',
                   returncode=run.returncode, first_error=error, log=f'{index}.log')
        if run.returncode == 0: row['object_sha256'] = hashlib.sha256(obj.read_bytes()).hexdigest()
        return row
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        pending = [pool.submit(compile_one, i, c) for i, c in enumerate(commands)]
        for future in as_completed(pending):
            report['units'].append(future.result()); save()
            if len(report['units']) % 10 == 0: print(f"audited {len(report['units'])}/{len(commands)}", flush=True)
    report['units'].sort(key=lambda r: r['source'])
    report.update(status='COMPLETE', compile_pass=sum(r['status']=='COMPILE_PASS' for r in report['units']),
                  compile_fail=sum(r['status']=='COMPILE_FAIL' for r in report['units']),
                  linked=False, runtime_executed=False, hardware_executed=False)
    save(); print(json.dumps({k:v for k,v in report.items() if k!='units'}), flush=True)

if __name__ == '__main__': main()
