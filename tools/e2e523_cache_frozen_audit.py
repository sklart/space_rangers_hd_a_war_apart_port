import pathlib,json,hashlib,re,sys
r=pathlib.Path(__file__).resolve().parents[1];f=r/'build/e2e516/hardware-e2e516-03651ce'
manifest=json.loads((f/'source-sha256.json').read_text());mapping=json.loads((f/'effective-source-map.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sys.path.insert(0,str(r/'tools'));from matrixgame_vector_object_overrides import function
src=r/mapping['upstream/cpp/src/EC_Cache.cpp'];text=src.read_text(encoding='utf-8')
key=str(src.relative_to(r)).replace('\\','/');expected=manifest[key]
result={'frozen_build':'03651ced28ecf0125f0b62c4f8129bd71e27f5c8 dirty b6c18a13cef88fe757d683e18cf48fc667067bda','cache_source':key,'expected_sha256':expected,'actual_sha256':sha(src),'matches_frozen_manifest':sha(src)==expected,'resource':'GI,Bm.FormRuins.2RCbg -> DATA/FormRuinsBG/2RCbg.gi; Format2/background.pkg','functions':{},'evict_calls':[]}
for marker in ['void TCacheControlEC::EvictData(', 'TCacheDataEC* TCacheEC::FindDataByKeyAndClass(', 'void EvictRuinsAndGovernmentCaches(', 'void EvictBlockChildrenFromCache(']:
    body=function(text,marker);result['functions'][marker]=body
for original,name in mapping.items():
    p=r/name
    if p.suffix!='.cpp' or not p.exists():continue
    t=p.read_text(encoding='utf-8')
    for num,line in enumerate(t.splitlines(),1):
        if 'EvictData(' in line or 'EvictBlockChildrenFromCache(' in line:
            result['evict_calls'].append({'file':name,'line':num,'text':line.strip(),'matches_frozen_manifest':sha(p)==manifest.get(name)})
result['conclusion']='EvictData defect exists in exact frozen cache source. Explicit retained two-control GI scenario applies to this resource type. Known Ruins eviction passes TCGaiEC; exact-class lookup excludes TCGiEC when temporary control is unbound. Runtime invocation on the defective GI and repeated-text causality remain unproven.'
assert result['matches_frozen_manifest']
assert all(x['matches_frozen_manifest'] for x in result['evict_calls'])
out=r/'reports/e2e5/hardware-20261023-cache-frozen-audit.json';out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ('functions','evict_calls')},ensure_ascii=False));print('calls',len(result['evict_calls']))
