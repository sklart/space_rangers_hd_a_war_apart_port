"""Create a separate link map to locate a preserved reference crash."""
import json
import subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
out = BASE / 'gov-frun-autosave-map'
if out.exists():
    raise RuntimeError('output already exists')
out.mkdir()
prior = BASE / 'gov-frun-autosave-link'
data = json.loads((prior / 'result.json').read_text(encoding='utf8'))
command = next(item['command'] for item in data['commands'] if item['label'] == 'link')[:]
command = [('/Fe' + str(out / 'mapped.exe')) if item.startswith('/Fe') else item for item in command]
command.append('/MAP:' + str(out / 'mapped.map'))
process = subprocess.run(command, cwd=ROOT, env=environment(out), capture_output=True, text=True, encoding='utf8', errors='replace')
(out / 'link.log').write_text(process.stdout + process.stderr, encoding='utf8')
if process.returncode:
    raise RuntimeError('map link failed')
print('map linked')
