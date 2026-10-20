"""Read-only fingerprints of user saves/archives; output only under build/."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/e2e515/data-baseline.json'


def snapshot():
    paths = set()
    saves = ROOT / 'saves'
    if saves.exists():
        paths.update(p for p in saves.rglob('*') if p.is_file())
    for pattern in ('*.sav', '*.bak', 'save.tmp'):
        paths.update(ROOT.glob(pattern))
    drive = Path('G:/')
    if drive.exists():
        paths.update(drive.glob('pack*.zip'))
    result = []
    for path in sorted(paths):
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(block)
        result.append({'path': str(path.resolve()), 'bytes': path.stat().st_size,
                       'sha256': digest.hexdigest()})
    return result


if __name__ == '__main__':
    entries = snapshot()
    if OUTPUT.exists():
        expected = json.loads(OUTPUT.read_text(encoding='utf-8'))['files']
        if entries != expected:
            raise SystemExit('FAIL: user data differs from baseline; baseline preserved')
        print(f'PASS: {len(entries)} original files unchanged')
    else:
        git = 'C:/Program Files/Git/cmd/git.exe'
        def run(*args):
            return subprocess.check_output([git, '-C', str(ROOT), *args], text=True).strip()
        try:
            remote = run('ls-remote', 'origin', 'refs/heads/master')
        except subprocess.CalledProcessError:
            remote = 'UNAVAILABLE: network lookup failed; cached ref is not live proof'
        report = {'head': run('rev-parse', 'HEAD'),
                  'origin_master_cached': run('rev-parse', 'origin/master'),
                  'origin_master_live': remote,
                  'status': run('status', '--short'),
                  'submodules': run('submodule', 'status'), 'files': entries}
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        with OUTPUT.open('x', encoding='utf-8') as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
        print(f'Baseline: {len(entries)} files; {OUTPUT}')
