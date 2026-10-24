"""Exercise the original RGB565 import wrapper with production resolver code."""
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from matrixgame_vector_object_overrides import function
from e2e_win32_host_test import implementation_sources


def main():
    out = ROOT / 'build/e2e523/ruins-import'
    out.mkdir(exist_ok=True)
    previous = ROOT / 'build/e2e523/ruins-draw'
    text = (previous / 'fixture.cpp').read_text(encoding='utf-8')
    main_source = (ROOT / 'upstream/cpp/src/GR_Main.cpp').read_text(encoding='utf-8')
    old = function(text, 'void Ex_OKGR_Copy_XY_XY_WORD(')
    wrappers = '\n'.join(function(main_source, sig) for sig in
                         ('void OKGR_Copy_XY_XY_WORD(', 'void Ex_OKGR_Copy_XY_XY_WORD('))
    text = text.replace(old, wrappers, 1)
    text = '#include "windows.hpp"\n#include "win32_compat_modules.hpp"\n' + text
    text = text.replace('  pas::free(target);', '''
  assert(srhd_awa::platform::win32_compat::ResolvedImportCount() == 1);
  assert(srhd_awa::platform::win32_compat::UnmappedImportCount() == 0);
  assert(srhd_awa::platform::win32_compat::PhysicalDllLoadCount() == 0);
  assert(srhd_awa::platform::win32_compat::DynamicModuleLoadCount() == 0);
  std::puts("original pas::win::load_import -> production ResolveImport -> OKGR RGB565 wrapper PASS; zero physical/dynamic DLL loads");
  pas::free(target);''', 1)
    source = out / 'fixture.cpp'
    source.write_text(text, encoding='utf-8')
    recipe_base = ROOT / 'build/e2e523/ruins-format2'
    command = json.loads((recipe_base / 'build.json').read_text(encoding='utf-8'))[-1]['command']
    command = [str(source) if s == str(recipe_base / 'fixture.cpp') else
               str(out / 'cachegi') if s == str(recipe_base / 'cachegi') else s for s in command]
    # Reused GI objects exercised Switch conditionals; platform services here
    # must use actual host implementations, selected by RESOLVER_TEST below.
    command = [s for s in command if not s.startswith('-D__SWITCH__')]
    command.insert(1, str(ROOT / 'port/switch/platform/win32_compat_okgf.cpp'))
    command[1:1] = ['-DE2E_HOST_RESOLVER_TEST', '-include', 'unistd.h',
                     '-I' + str(ROOT / 'port/switch/build/e2e5-host-game/optimized/runtime-overlay')]
    command += implementation_sources() + ['-lz']
    completed = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(completed.stdout + completed.stderr, encoding='utf-8')
    print('compile', completed.returncode, flush=True)
    if completed.returncode:
        print(completed.stderr[-6000:])
        raise SystemExit(completed.returncode)
    import e2e5_gamesettings_rescale as check
    code = (ROOT / 'tools/e2e5_gamesettings_rescale.py').read_text().replace('"forms.pkg"', '"background.pkg"').replace('if not re.fullmatch(', 'if not re.search(')
    exec(compile(code, check.__file__, 'exec'), check.__dict__)
    from probe_gai_release import decode2
    check.decode_format0 = lambda d: (decode2(d)[0]['decoded'], decode2(d)[1])
    check.RESOURCE = 'data/formruinsbg/2rcbg.gi'
    result = check.run(ROOT / 'windows/Space Rangers HD A War Apart',
                       ROOT / 'port/switch/build/e2e5-host-game/switch-effective/okgf/libokgf.so', 1280, 720,
                       original_graphbuf=ROOT / 'build/e2e523/ruins-format2/graphbuf', original_cachegi=out / 'cachegi')
    assert 'original pas::win::load_import -> production ResolveImport' in result['original_cachegi_output']
    result.update(boundary='HOST actual import wrapper and production resolver; full UI/ARM64/hardware NOT RUN',
                  link_command=command,
                  source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                 for p in [source, ROOT / 'upstream/cpp/src/GR_Main.cpp',
                                           ROOT / 'port/switch/platform/win32_compat.cpp']})
    result['reused_objects_sha256'] = {s: hashlib.sha256(pathlib.Path(s).read_bytes()).hexdigest()
                                       for s in command if s.endswith('.o')}
    result['executable_sha256'] = hashlib.sha256((out / 'cachegi').read_bytes()).hexdigest()
    (out / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(result['original_cachegi_output'], flush=True)


if __name__ == '__main__':
    main()
