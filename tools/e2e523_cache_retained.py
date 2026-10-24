"""Observe original cache eviction with a real loaded GI and a retained peer."""
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))


def main():
    out = ROOT / 'build/e2e523/cache-retained'
    out.mkdir(exist_ok=True)
    previous = ROOT / 'build/e2e523/ruins-draw'
    text = (previous / 'fixture.cpp').read_text(encoding='utf-8')
    text = '#include "layout/EC_Cache.hpp"\n#include "units/EC_Cache.hpp"\n' + text
    text = text.replace('namespace GR_Main {', 'namespace GR_Main {\nEC_Cache::TCacheEC* GlobalCache{};', 1)
    test = '''
  auto* manager=pas::construct_call<EC_Cache::TCacheEC>(EC_Cache::TCacheEC_Create);
  GR_Main::GlobalCache=manager;
  manager->AddDataToLruHead(cache);manager->ResidentBytes=cache->ResidentBytes;
  auto* held=pas::construct_call<EC_Cache::TCacheControlEC>(EC_Cache::TCacheControlEC_Create);
  auto* temporary=pas::construct_call<EC_Cache::TCacheControlEC>(EC_Cache::TCacheControlEC_Create);
  held->CacheKey=cache->CacheKey;temporary->CacheKey=cache->CacheKey;
  held->BoundData=cache;held->RetainCount=1;cache->AppendControl(held);
  temporary->BoundData=cache;cache->AppendControl(temporary);
  temporary->EvictData(pas::class_ref<EC_CacheGI::TCGiEC>());
  const bool freed_retained=held->BoundData==nullptr && held->RetainCount==1 && manager->ResidentBytes==0 && manager->MostRecentData==nullptr;
  std::printf("retained peer invalidated=%d retain=%d bytes=%d; original EvictData ownership observation\\n",freed_retained,held->RetainCount,manager->ResidentBytes);
  if(!freed_retained)return 41;
  // EvictData has destroyed the actual GI; do not dereference its old address.
  cache=nullptr;
  pas::free(temporary);pas::free(held);pas::free(manager);GR_Main::GlobalCache=nullptr;
'''
    text = text.replace('  pas::free(cache);', test + '\n  pas::free(cache);', 1)
    source = out / 'fixture.cpp'
    source.write_text(text, encoding='utf-8')
    recipe_base = ROOT / 'build/e2e523/ruins-format2'
    command = json.loads((recipe_base / 'build.json').read_text())[-1]['command']
    command = [str(source) if s == str(recipe_base / 'fixture.cpp') else
               str(out / 'cachegi') if s == str(recipe_base / 'cachegi') else s for s in command]
    command.insert(1, str(ROOT / 'port/switch/platform/win32_compat_okgf.cpp'))
    command.insert(1, str(ROOT / 'upstream/cpp/src/GlobalsV.cpp'))
    p = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(p.stdout + p.stderr)
    print('compile', p.returncode, flush=True)
    if p.returncode:
        print(p.stderr[-4000:])
        raise SystemExit(p.returncode)
    import e2e5_gamesettings_rescale as check
    code = (ROOT / 'tools/e2e5_gamesettings_rescale.py').read_text().replace('"forms.pkg"', '"background.pkg"').replace('if not re.fullmatch(', 'if not re.search(')
    exec(compile(code, check.__file__, 'exec'), check.__dict__)
    from probe_gai_release import decode2
    check.decode_format0 = lambda d: (decode2(d)[0]['decoded'], decode2(d)[1])
    check.RESOURCE = 'data/formruinsbg/2rcbg.gi'
    result = check.run(ROOT / 'windows/Space Rangers HD A War Apart',
                       ROOT / 'port/switch/build/e2e5-host-game/switch-effective/okgf/libokgf.so', 1280, 720,
                       original_graphbuf=recipe_base / 'graphbuf', original_cachegi=out / 'cachegi')
    assert 'retained peer invalidated=1' in result['original_cachegi_output']
    result.update(boundary='Original cache ownership defect reproduced with explicit two-control setup; occurrence in hardware session unproven',
                  link_command=command, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    (out / 'result.json').write_text(json.dumps(result, indent=2))
    print(result['original_cachegi_output'], flush=True)


if __name__ == '__main__':
    main()
