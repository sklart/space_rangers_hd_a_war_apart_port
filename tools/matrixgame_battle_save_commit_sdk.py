"""Build the two production units requiring a completed pre-battle save."""
import hashlib, json, os, shutil, subprocess
from pathlib import Path
from e2e_source_overrides import generated_source
from matrixgame_legacy_wav_sdk_dependency import members

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/matrixgame/battle-save-commit-sdk-2'
old = ROOT / 'build/matrixgame/gov-autosave-sdk-2/liboriginal-rangers-host-govsave.a'
overlay = ROOT / 'build/matrixgame/snapshot-prep-overlay-audit/source-overlay'
if out.exists(): raise RuntimeError('output already exists')
out.mkdir()
env = dict(os.environ, DEVKITPRO='C:/devkitPro', DEVKITA64='C:/devkitPro/devkitA64')
result = {'status': 'INCOMPLETE', 'commands': []}
def sha(p):
    with p.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()
def run(label, args):
    args = list(map(str, args)); p = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (out / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'exit': p.returncode, 'command': args})
    print(label, p.returncode, flush=True)
    if p.returncode: raise RuntimeError('see ' + label + '.log')
try:
    flags = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf8'))
    objcopy = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-objcopy.exe'
    ar = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe'
    baseline = {'aSaveLoad': ROOT / 'build/matrixgame/snapshot-prep-sdk-1/aSaveLoad.cpp',
                'fGov': ROOT / 'build/matrixgame/gov-autosave-sdk-2/fGov.cpp'}
    before = members(old); old_hash = sha(old); objects = []
    for unit, previous_source in baseline.items():
        # Extract the exact old member without changing the retained archive.
        raw=old.read_bytes(); assert raw[:8]==b'!<arch>\n'; offset=8; names=b''; payload=None
        while offset<len(raw):
            entry=raw[offset:offset+60]; assert entry[58:]==b'`\n'
            name=entry[:16].decode('ascii').strip(); size=int(entry[48:58]); body=raw[offset+60:offset+60+size]
            if name=='//': names=body
            elif name not in ('/', '/SYM64/'):
                if name.startswith('/'):
                    start=int(name[1:]); name=names[start:names.index(b'/\n',start)].decode()
                else: name=name.rstrip('/')
                if name==unit+'.o': assert payload is None; payload=body
            offset+=60+size+size%2
        assert payload is not None
        old_obj = out / ('old-' + unit + '.o'); old_obj.write_bytes(payload); assert sha(old_obj) == before[unit + '.o']
        reconstructed = out / ('baseline-' + unit + '.o')
        run('compile-baseline-' + unit, [flags['compiler'], *flags['flags'], '-c', previous_source, '-o', reconstructed])
        for label, path in (('old', old_obj), ('baseline', reconstructed)):
            run('strip-' + label + '-' + unit, [objcopy, '--strip-debug', path, out / (label + '-' + unit + '-stripped.o')])
        assert sha(out / ('old-' + unit + '-stripped.o')) == sha(out / ('baseline-' + unit + '-stripped.o'))
        source = out / (unit + '.cpp')
        generated_source(overlay / (unit + '.cpp'), source, audio_on=True, e2e5_profile=True, script_width=True)
        obj = out / (unit + '.o'); run('compile-production-' + unit, [flags['compiler'], *flags['flags'], '-c', source, '-o', obj]); objects.append(obj)
    archive = out / 'liboriginal-rangers-host-battlesave.a'; shutil.copyfile(old, archive)
    run('replace-members', [ar, 'rs', archive, *objects]); after = members(archive)
    changed = sorted(name for name in before if before[name] != after[name])
    assert set(before) == set(after) and changed == ['aSaveLoad.o', 'fGov.o'] and sha(old) == old_hash
    assert all(after[obj.name] == sha(obj) for obj in objects)
    result.update(status='PASS; two production SDK objects rebuilt; hardware NOT RUN', archive=str(archive), archive_sha256=sha(archive), old_archive_sha256=old_hash, changed_members=changed, sources_sha256={unit:sha(out / (unit + '.cpp')) for unit in baseline})
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
