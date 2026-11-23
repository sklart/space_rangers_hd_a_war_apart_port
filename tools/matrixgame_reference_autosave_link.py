"""Build a separate Windows reference with the original Gov start handler."""
import hashlib
import json
import subprocess
from pathlib import Path

from matrixgame_windows_host_env import environment

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    out = BASE / 'gov-frun-autosave-link-2'
    if out.exists():
        raise RuntimeError('output already exists')
    out.mkdir()
    env = environment(out)
    prior = BASE / 'gov-frun-link-3'
    old = json.loads((prior / 'result.json').read_text(encoding='utf8'))
    result = {'status': 'INCOMPLETE', 'commands': []}

    def run(label, command):
        command = list(map(str, command))
        process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True,
                                 text=True, encoding='utf8', errors='replace')
        (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
        result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
        print(label, process.returncode, flush=True)
        if process.returncode:
            raise RuntimeError('see ' + label + '.log')

    try:
        source_text = (BASE / 'gov-frun-two-link/campaign_frun_two.cpp').read_text(encoding='utf8')
        source_text = source_text.replace('#include "units/fGov.hpp"', '#include "units/fGov.hpp"\n#include "types/fGalaxy2.hpp"')
        initial = 'auto* gov=Globals::GovernmentScreen;gov->InitializeLayout();'
        assert source_text.count(initial) == 1
        source_text = source_text.replace(initial, 'Globals::GalaxyScreen->InitializeLayout();' + initial)
        old_entry = 'gov->PlanetBattleMapId=map_id;fGov::GovernmentBattleDifficulty=difficulty;gov->PendingTransition=1;'
        new_entry = ('assert(difficulty==3);gov->StartPlanetBattleWithBombardment(map_id);'
                     'assert(gov->PlanetBattleMapId==map_id && fGov::GovernmentBattleDifficulty==difficulty && gov->PendingTransition==1);')
        assert source_text.count(old_entry) == 1
        source_text = source_text.replace(old_entry, new_entry)
        source_text = source_text.replace('ui_autosave_handler=NOT_RUN', 'ui_autosave_handler=CALLED')
        source_text = source_text.replace('no_save_file_API=PASS', 'ui_autosave_handler=CALLED')
        source = out / 'campaign_frun_autosave.cpp'
        source.write_text(source_text, encoding='utf8')
        (out / 'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="' + sha(source) + '";\n', encoding='utf8')
        entry = next(x for x in old['commands'] if x['label'] == 'compile-campaign_frun')
        command = entry['command'][:]
        command.insert(1, '/I' + str(out))
        command[command.index('/c') + 1] = str(source)
        obj = out / 'campaign_frun_autosave.obj'
        command = [('/Fo' + str(obj)) if x.startswith('/Fo') else x for x in command]
        run('compile', command)
        old_obj = prior / 'campaign_frun.obj'
        items = (prior / 'link.rsp').read_text(encoding='utf8').splitlines()
        assert sum(line.strip('"') == str(old_obj) for line in items) == 1
        rsp = out / 'link.rsp'
        rsp.write_text('\n'.join('"' + str(obj) + '"' if x.strip('"') == str(old_obj) else x for x in items), encoding='utf8')
        link = next(x for x in old['commands'] if x['label'] == 'link')['command'][:]
        binary = out / 'gov-frun-autosave.exe'
        link = [('@' + str(rsp)) if x.startswith('@') else ('/Fe' + str(binary)) if x.startswith('/Fe') else x for x in link]
        run('link', link)
        result.update(status='PASS; handler variant linked; runtime NOT RUN', source_sha256=sha(source), binary_sha256=sha(binary))
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

if __name__ == '__main__':
    main()
