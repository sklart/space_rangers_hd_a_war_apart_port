"""Compile actual effective input/message-loop units, without making a test NRO."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'port/switch/build/e2e-game-save-test'
OUTPUT = ROOT / 'build/e2e515/sdk-input'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = OUTPUT / 'result.json'
    report.write_text(json.dumps({'status': 'RUNNING'}))
    config = json.loads((BUILD / 'compile-flags.txt').read_text())
    flags = config['flags']
    if '-D__SWITCH__' not in flags or '-DE2E4_QUICKSAVE_EXPERIMENT=1' not in flags:
        raise RuntimeError('Expected the real Save ON Switch compilation flags')
    subprocess.run([sys.executable, str(ROOT / 'tools/audit_win32_surface.py'), '--effective',
                    '--build-root', str(BUILD), '--manifest',
                    str(ROOT / 'win32-effective-save-test-import-manifest.json'), '--check'],
                   cwd=ROOT, check=True, timeout=120)
    mapping = json.loads((BUILD / 'effective-source-map.json').read_text())
    loop = ROOT / mapping['upstream/cpp/src/GI_MessageLoop.cpp']
    edit = ROOT / mapping['upstream/cpp/src/GI_Edit.cpp']
    arcade = ROOT / mapping['upstream/cpp/src/ab_MainForm.cpp']
    text = loop.read_text()
    if text.count('InputContextScope e2e_input_owner(') != 2 or text.count('e2e_input_owner.CancelHeldInput();') != 2:
        raise RuntimeError('Both real loops must enter and cancel scoped input')
    for hook, expected in (('controller_ui::Scope e2e_controller_ui(this);', 2),
                           ('controller_ui::Process(this, Message, WParam, LParam)', 1),
                           ('controller_ui::Draw(this);', 1)):
        if text.count(hook) != expected:
            raise RuntimeError(f'Native focus hook missing or duplicated: {hook}')
    if 'controller_text_entry::Open(this);' not in edit.read_text():
        raise RuntimeError('Generic TEditGI keyboard is not in the effective source')
    results = []
    for source in (ROOT / 'port/switch/platform/input_platform.cpp',
                   ROOT / 'port/switch/platform/win32_compat_messages.cpp', loop, edit, arcade):
        obj = OUTPUT / (source.stem + '.o')
        command = [config['compiler'], *flags, '-c', str(source), '-o', str(obj)]
        result = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=300)
        (OUTPUT / (source.stem + '.log')).write_text(result.stdout, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(f'{source.name}: compile exit {result.returncode}; see {OUTPUT}')
        results.append({'source': str(source), 'source_sha256': digest(source),
                        'object_sha256': digest(obj), 'object_bytes': obj.stat().st_size})
        print(f'{source.name}: SDK COMPILE PASS', flush=True)
    headers = {name: digest(ROOT / 'port/switch/platform' / name) for name in
               ('controller_actions.hpp', 'controller_screen_context.hpp',
                'input_platform.hpp', 'win32_compat_messages.hpp', 'e2e5_text_input.hpp',
                'controller_text_input.hpp', 'controller_text_entry.hpp',
                'controller_profiles.hpp', 'controller_focus.hpp',
                'controller_messages.hpp', 'controller_ui.hpp')}
    report.write_text(json.dumps({'status': 'COMPILE PASS', 'linked': False, 'hardware_executed': False,
                                 'compiler': config['compiler'], 'flags': flags,
                                 'headers_sha256': headers, 'objects': results}, indent=2) + '\n')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        OUTPUT.mkdir(parents=True, exist_ok=True)
        (OUTPUT / 'result.json').write_text(json.dumps({'status': 'FAIL', 'error': str(error)}, indent=2) + '\n')
        raise
