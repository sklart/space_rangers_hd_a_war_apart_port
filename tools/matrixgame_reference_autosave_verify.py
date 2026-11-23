"""Verify the preserved handler run after correcting its save-directory check."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123/gov-frun-autosave-runtime-2'
prior = json.loads((out / 'result.json').read_text(encoding='utf8'))
log = (out / 'runtime.log').read_text(encoding='utf8', errors='replace')
assert prior['exit'] == 0
assert log.count('entry_boundary=OnOpen map_id=12 difficulty=3 real_campaign_planet=PASS ui_autosave_handler=CALLED') == 2
assert log.count('original_Gov_history=PASS actual_statistics=PASS') == 2
assert log.count('CALLER_LEASE natural_return_window_owned=PASS') == 2
assert all('GOV_FRUN battle_index=' + str(i) + ' completed' in log for i in (1, 2))
assert 'original_campaign_UI_script_host_cleanup=PASS' in log

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

assert all(sha(Path(path)) == digest for path, digest in prior['fixture_sha256'].items())
directory = out / 'output/rangers-user/config/save'
files = [directory / 'AutoSave.sav', directory / 'AutoSave.sav.bak']
assert all(path.is_file() and path.stat().st_size > 0 for path in files)
result = {'status': 'PASS; two handler-driven battles and autosave files; reload and Switch NOT RUN',
          'original_verifier': 'FAIL: checked rangers-user/save rather than rangers-user/config/save',
          'runtime_exit': prior['exit'],
          'files': [{'path': str(path), 'size': path.stat().st_size, 'sha256': sha(path)} for path in files]}
target = out / 'verification-corrected.json'
if target.exists():
    raise RuntimeError('corrected verification already exists')
target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
print(json.dumps(result, indent=2))
