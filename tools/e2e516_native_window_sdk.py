"""Build an isolated archive that uses one native VI window for E2E516."""
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

from e2e516_native_window_overrides import apply
from matrixgame_legacy_wav_sdk_dependency import members

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/integrated-grmain-sdk-20261124-1'
OUT = ROOT / 'build/matrixgame/e2e516-native-window-sdk-20261124-1'
AR = Path('C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    if OUT.exists():
        raise RuntimeError('Output exists; previous evidence must be preserved')
    prior = json.loads((BASE / 'result.json').read_text(encoding='utf-8'))
    source_archive = Path(prior['archive'])
    if sha(source_archive) != prior['archive_sha256']:
        raise RuntimeError('Previous archive hash changed')
    old_gr = BASE / 'GR_Main.cpp'
    if sha(old_gr) != prior['source_sha256']:
        raise RuntimeError('Previous generated GR_Main source changed')
    OUT.mkdir(parents=True)
    commands = []
    def run(label, command):
        command = list(map(str, command))
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
        (OUT / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'label': label, 'command': command, 'exit': result.returncode})
        print(label, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError('See ' + label + '.log')
    result = {'status': 'INCOMPLETE', 'commands': commands}
    try:
        gr = OUT / 'GR_Main.cpp'
        gr.write_text(apply(old_gr.read_text(encoding='utf-8')), encoding='utf-8', newline='\n')
        prior_command = next(c['command'] for c in prior['commands'] if c['label'] == 'compile-portable')[:]
        prior_command[prior_command.index('-c')+1] = str(gr)
        prior_command[prior_command.index('-o')+1] = str(OUT / 'GR_Main.o')
        run('compile-grmain', prior_command)
        flags = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf-8'))
        runtime = ROOT / 'port/switch/platform/runtime_platform.cpp'
        run('compile-runtime', [flags['compiler'], *flags['flags'], '-c', runtime, '-o', OUT / 'runtime_platform.o'])
        archive = OUT / 'liboriginal-rangers-host-native-window.a'
        shutil.copyfile(source_archive, archive)
        old_members = members(archive)
        run('replace', [AR, 'rs', archive, OUT / 'GR_Main.o', OUT / 'runtime_platform.o'])
        new_members = members(archive)
        changed = sorted(k for k in old_members if old_members[k] != new_members[k])
        if set(old_members) != set(new_members) or changed != ['GR_Main.o', 'runtime_platform.o']:
            raise RuntimeError('Unexpected archive member changes: ' + repr(changed))
        if sha(source_archive) != prior['archive_sha256']:
            raise RuntimeError('Previous archive was modified')
        result.update(status='PASS SDK compile/archive; hardware NOT RUN', archive=str(archive),
                      archive_sha256=sha(archive), changed_members=changed,
                      source_sha256=sha(gr), runtime_sha256=sha(runtime), archive_members_sha256=new_members)
    finally:
        (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')

if __name__ == '__main__':
    main()
