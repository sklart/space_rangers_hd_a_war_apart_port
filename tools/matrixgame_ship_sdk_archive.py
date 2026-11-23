"""Rebuild the saved-game galaxy unit in a separate Switch host archive."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from matrixgame_campaign_cleanup_overrides import apply as cleanup
from matrixgame_campaign_snapshot_overrides import apply as snapshot_cleanup
from matrixgame_legacy_wav_sdk_dependency import members

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/matrixgame/ship-reference-sdk-3'
old = ROOT / 'build/matrixgame/e2e610-native-ui-pump/sdk/6/liboriginal-rangers-host-native-ui.a'
if out.exists():
    raise RuntimeError('output already exists')
out.mkdir()
env = dict(os.environ, DEVKITPRO='C:/devkitPro', DEVKITA64='C:/devkitPro/devkitA64')
result = {'status': 'INCOMPLETE', 'commands': []}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(label, command):
    command = list(map(str, command))
    process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
    print(label, process.returncode, flush=True)
    if process.returncode:
        raise RuntimeError('see ' + label + '.log')

try:
    flags = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf8'))
    mapping = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/effective-source-map.json').read_text(encoding='utf8'))
    original = ROOT / mapping['upstream/cpp/src/aGalaxy.cpp']
    source = out / 'aGalaxy.cpp'
    source.write_text(snapshot_cleanup('aGalaxy.cpp', cleanup('aGalaxy.cpp', original.read_text(encoding='utf8'))), encoding='utf8')
    header = ROOT / 'port/switch/platform/e2e5_save_inventory.hpp'
    text = header.read_text(encoding='utf8')
    begin = '    // A script can retain a reference to a live ship owned by the galaxy\n'
    end = '    if (auto* item = e2e515_item_references::Find(galaxy, value))\n'
    assert text.count(begin) == text.count(end) == 1
    before, rest = text.split(begin, 1)
    _, after = rest.split(end, 1)
    baseline = out / 'baseline'; baseline.mkdir()
    (baseline / 'e2e5_save_inventory.hpp').write_text(before + end + after, encoding='utf8')
    item_header = ROOT / 'port/switch/platform/e2e515_item_references.hpp'
    item_text = item_header.read_text(encoding='utf8')
    added_includes = '#include "types/fEquipmentShop.hpp"\n#include "units/fEquipmentShop.hpp"\n'
    added_drop = '''        for (int j = 0; !found && j < count(star->MovingDropItems); ++j) {
            auto* drop = pas::list_at<aGalaxy::TMovingDropItemEntry>(star->MovingDropItems, j);
            if (drop && drop->Payload)
                item(pas::class_cast_if<aItem::TItem*>(drop->Payload));
        }
'''
    added_shop = '''    for (int i = 0; !found && i < count(fEquipmentShop::TemporaryShopSlots); ++i) {
        auto* slot = pas::list_at<fEquipmentShop::TShopSlot>(fEquipmentShop::TemporaryShopSlots, i);
        if (slot) item(slot->Item);
    }
'''
    for addition in (added_includes, added_drop, added_shop):
        assert item_text.count(addition) == 1
        item_text = item_text.replace(addition, '', 1)
    (baseline / 'e2e515_item_references.hpp').write_text(item_text, encoding='utf8')
    old_hash = sha(old)
    ar = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe'
    objcopy = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-objcopy.exe'
    # Read raw archive bytes: the Windows ar executable writes binary stdout
    # through a translating stream, which corrupts ELF members containing LF.
    raw = old.read_bytes(); assert raw[:8] == b'!<arch>\n'
    offset = 8; names = b''; payload = None
    while offset < len(raw):
        entry = raw[offset:offset + 60]; assert entry[58:] == b'`\n'
        name = entry[:16].decode('ascii').strip(); size = int(entry[48:58])
        body = raw[offset + 60:offset + 60 + size]
        if name == '//': names = body
        elif name not in ('/', '/SYM64/'):
            if name.startswith('/'):
                start = int(name[1:]); name = names[start:names.index(b'/\n', start)].decode()
            else: name = name.rstrip('/')
            if name == 'aGalaxy.o': assert payload is None; payload = body
        offset += 60 + size + (size % 2)
    assert payload is not None
    old_obj = out / 'original-aGalaxy.o'; old_obj.write_bytes(payload)
    assert sha(old_obj) == members(old)['aGalaxy.o']
    baseline_obj = out / 'baseline-aGalaxy.o'
    run('compile-baseline', [flags['compiler'], '-I' + str(baseline), *flags['flags'], '-c', source, '-o', baseline_obj])
    for name, obj in (('old', old_obj), ('baseline', baseline_obj)):
        run('strip-' + name, [objcopy, '--strip-debug', obj, out / (name + '-stripped.o')])
    result['baseline_stripped_matches_old'] = sha(out / 'old-stripped.o') == sha(out / 'baseline-stripped.o')
    result['baseline_old_sha256'] = sha(old_obj)
    result['baseline_rebuilt_sha256'] = sha(baseline_obj)
    if not result['baseline_stripped_matches_old']:
        raise RuntimeError('reconstructed baseline differs from retained SDK member; inspect before replacing')
    obj = out / 'aGalaxy.o'
    run('compile-fixed', [flags['compiler'], *flags['flags'], '-c', source, '-o', obj])
    archive = out / 'liboriginal-rangers-host-shipfix.a'
    shutil.copyfile(old, archive)
    run('replace-member', [ar, 'rs', archive, obj])
    before_members, after_members = members(old), members(archive)
    assert set(before_members) == set(after_members)
    changed = sorted(name for name in before_members if before_members[name] != after_members[name])
    assert changed == ['aGalaxy.o'] and after_members['aGalaxy.o'] == sha(obj) and sha(old) == old_hash
    result.update(status='PASS; one verified SDK archive member rebuilt; Switch NOT RUN', archive=str(archive),
                  archive_sha256=sha(archive), old_archive_sha256=old_hash, changed_members=changed,
                  source_sha256=sha(source), header_sha256=sha(header), item_header_sha256=sha(item_header))
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
