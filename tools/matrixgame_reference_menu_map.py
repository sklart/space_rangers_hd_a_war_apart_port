"""Build a separate symbol map for the live Windows menu reference."""
import json
import subprocess
import sys
from pathlib import Path
from matrixgame_windows_host_env import environment

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
missing = sys.argv[1:] == ['--missing-map']
item_trace_v3 = sys.argv[1:] == ['--item-trace-v3']
item_trace = sys.argv[1:] == ['--item-trace'] or item_trace_v3
if sys.argv[1:] and not (missing or item_trace):
    raise RuntimeError('unknown arguments')
out = BASE / ('gov-frun-item-trace-map-2' if item_trace_v3 else 'gov-frun-item-trace-map-1' if item_trace else 'gov-frun-missing-map-map-2' if missing else 'gov-frun-menu-map-1')
if out.exists():
    raise RuntimeError('output already exists')
out.mkdir()
data = json.loads((BASE / ('gov-frun-gov-item-trace-link-3' if item_trace_v3 else 'gov-frun-gov-item-trace-link-2' if item_trace else 'gov-frun-missing-map-link-2' if missing else 'gov-frun-menu-link-1') / 'result.json').read_text(encoding='utf8'))
command = next(item['command'] for item in data['commands'] if item['label'] == 'link')[:]
command = [('/Fe' + str(out / 'mapped.exe')) if item.startswith('/Fe') else item for item in command]
command.append('/MAP:' + str(out / 'mapped.map'))
process = subprocess.run(command, cwd=ROOT, env=environment(out), capture_output=True, text=True, encoding='utf8', errors='replace')
(out / 'link.log').write_text(process.stdout + process.stderr, encoding='utf8')
if process.returncode:
    raise RuntimeError('map link failed')
print('map linked')
