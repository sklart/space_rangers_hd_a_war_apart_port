"""Read-only audit of the frozen E2E516 bundle and function-level source differences.

Preprocessed inputs must be produced with the saved host/ARM compile flags.
Does not build or replace the game, regenerate effective sources or touch saves.
"""
import difflib
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

from ci_stage_switch_artifacts import matching_build_id
from e2e_arm64_build import dependency_paths

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/e2e5161'
def sha(data):return hashlib.sha256(data).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8'))

def body(text, signature):
    start=text.index(signature)
    opening=text.index('{',start)
    # Ignore braces inside literals/comments when extracting the real body.
    tokens=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[{}]',re.S)
    depth=0
    for m in tokens.finditer(text,opening):
        if m[0]=='{':depth+=1
        elif m[0]=='}':
            depth-=1
            if not depth:return text[start:m.end()]
    raise ValueError('Incomplete function: '+signature)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    host=read(ROOT/'build/e2e516/tested-host-composition.json')
    provenance=read(ROOT/'reports/e2e5/e2e516-artifact-provenance.json')
    bundle=Path(provenance['bundle']['path'])
    assert sha(bundle.read_bytes())==provenance['bundle']['sha256']
    with zipfile.ZipFile(bundle) as z:
        assert z.testzip() is None
        manifest=json.loads(z.read('manifest.json'))
        build=json.loads(z.read('build-report.json'))
        mapping=json.loads(z.read('effective-source-map.json'))
        inputs=json.loads(z.read('source-sha256.json'))
        for line in z.read('SHA256SUMS').decode().splitlines():
            digest,name=line.split('  ',1)
            assert sha(z.read(name))==digest,name
        for name,digest in inputs.items():
            assert sha((ROOT/name).read_bytes())==digest,name
        nro=z.read('switch/space-rangers-hd-a-war-apart/SpaceRangersHD-perf516.nro')
        elf=z.read('debug/SpaceRangersHD-perf516.elf')
        assert matching_build_id(nro,elf)==manifest['build_id']==provenance['build_id']
        assert manifest['source_commit']==build['build_git']==provenance['source_commit']
        assert ('build_git='+build['build_git'][:7]).encode() in nro
        for name,data in [('switch/space-rangers-hd-a-war-apart/SpaceRangersHD-perf516.nro',nro),('debug/SpaceRangersHD-perf516.elf',elf)]:
            assert sha(data)==manifest['artifacts'][name]['sha256']==provenance['artifacts'][name]['sha256']

    b=ROOT/'port/switch/build/e2e-game-perf515'
    linked=[Path(line.strip().strip('"')) for line in (b/'link-objects.rsp').read_text().splitlines() if line.strip()]
    assert len(linked)==build['link_objects']
    actual=[]
    by_stem={Path(k).stem:(k,v) for k,v in mapping.items()}
    for obj in linked:
        deps=dependency_paths(obj.with_suffix('.d'))
        assert deps,obj
        if obj.parent.name=='upstream':
            original,effective=by_stem[obj.stem]
            assert (ROOT/effective) in deps,(obj,effective)
            actual.append({'original':original,'effective':effective,'object':str(obj.relative_to(ROOT))})
    assert len(actual)==297
    assert not any(x['original'].endswith('/EC_HsFile.cpp') for x in actual)
    assert any(x.name=='ec_file_adapter.o' for x in linked)
    assert not any('port/matrixgame/' in x.as_posix().lower() for x in linked)
    unrelated=['reports/e2e5/e2e510-artifact-provenance.json','saves/']
    assert not any(name==unrelated[0] or name.startswith(unrelated[1]) for name in inputs)

    functions={}
    for name in ('GR_Main.cpp','ab_StopLine.cpp'):
        key='upstream/cpp/src/'+name
        old_path=host['source_map'][key]
        old=(ROOT/old_path).read_bytes()
        assert sha(old)==host['compiled_sources_sha256'][old_path],old_path
        new=(ROOT/mapping[key]).read_bytes()
        assert sha(new)==inputs[mapping[key]]
        old_text=old.decode().replace('\r\n','\n');new_text=new.decode().replace('\r\n','\n')
        diff=''.join(difflib.unified_diff(old_text.splitlines(True),new_text.splitlines(True),fromfile='tested-host/'+name,tofile='release-arm/'+name))
        (OUT/(name+'.diff')).write_text(diff)
        functions[name]={'host_effective':old_path,'arm_effective':mapping[key],'host_sha256':sha(old),'arm_sha256':sha(new),'identical':old==new,'line_endings_differ':(b'\r\n' in old)!=(b'\r\n' in new),'diff':'build/e2e5161/'+name+'.diff'}
    # Prove the complete StopLine difference, not merely that its filename was
    # allowed: reconstruct the original expression at exactly the two call sites.
    from e2e516_stop_overrides import reference
    old=(ROOT/host['source_map']['upstream/cpp/src/ab_StopLine.cpp']).read_text()
    new=(ROOT/mapping['upstream/cpp/src/ab_StopLine.cpp']).read_text()
    assert reference(new)==old
    before=body(old,'    void ab_StopLine_GetDistances(')
    after=body(new,'    void ab_StopLine_GetDistances(')
    (OUT/'ab_StopLine_GetDistances.diff').write_text(''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True))))

    pre={k:(OUT/f'GR_Main-{k}.ii').read_text() for k in ('host','arm64')}
    selected=('GetGameUserDirectory','GR_DXInit','FinalizeRuntimeAndSettings','PresentScreenBuffer','CaptureScreenBackground','EndFramePresentation','AppendLogLineThreadSafe','AppendDebugLogLine','AppendLogTextThreadSafe','LogMemoryUsage')
    pre_functions={}
    for name in selected:
        sig=('    pas::WideString ' if name=='GetGameUserDirectory' else '    void ')+name+'('
        chunks={k:body(v[v.rfind('\nnamespace GR_Main {'):],sig) for k,v in pre.items()}
        same=chunks['host']==chunks['arm64']
        pre_functions[name]={'identical_preprocessed_body':same,'sha256':{k:sha(v.encode()) for k,v in chunks.items()}}
        (OUT/('preprocessed-'+name+'.diff')).write_text(''.join(difflib.unified_diff(chunks['host'].splitlines(True),chunks['arm64'].splitlines(True))))

    def definitions(text):
        text=text[text.rfind('\nnamespace GR_Main {'):]
        result={}
        for m in re.finditer(r'^    \S[^\n{};]*\([^\n{};]*\) \{',text,re.M):
            signature=m[0][:-1].rstrip()
            if signature in result:raise ValueError('Duplicate function signature: '+signature)
            result[signature]=body(text,signature)
        return result
    definitions_by_target={k:definitions(v) for k,v in pre.items()}
    names=set(definitions_by_target['host'])|set(definitions_by_target['arm64'])
    differing=[name.strip() for name in sorted(names) if definitions_by_target['host'].get(name)!=definitions_by_target['arm64'].get(name)]
    pre_all={'host_function_definitions':len(definitions_by_target['host']),
             'arm_function_definitions':len(definitions_by_target['arm64']),
             'differing_bodies':differing,
             'scope':'Top-level single-line function definitions in the actual preprocessed GR_Main namespace; header implementation ABI differences remain separate.'}

    git='C:/Program Files/Git/cmd/git.exe'
    changes=subprocess.check_output([git,'diff','--name-only',build['build_git']],cwd=ROOT,text=True).splitlines()
    status=subprocess.check_output([git,'status','--porcelain','--untracked-files=all'],cwd=ROOT,text=True).splitlines()
    result={'status':'ARTIFACT_AND_FUNCTION_AUDIT_PASS','hardware_executed':False,'rebuilt':False,
            'source_git':build['build_git'],'build_dirty':build['build_dirty'],
            'current_changes_since_source_commit':changes,'current_untracked':[x[3:] for x in status if x.startswith('?? ')],
            'bundle_sha256':provenance['bundle']['sha256'],'build_id':manifest['build_id'],
            'verified_dependency_inputs':len(inputs),'linked_objects':len(linked),
            'linked_original_game_units':len(actual),'linked_platform_units':len(linked)-len(actual),
            'linked_original_sources':actual,'source_differences':functions,
            'preprocessed_functions':pre_functions,
            'all_preprocessed_GR_Main_functions':pre_all,
            'stopline_entire_tu_reference_restoration_exact':True,
            'unrelated_user_paths_absent_from_compilation':True,
            'note':'Artifact/hash/branch audit; no GPU execution or Switch runtime claim.'}
    (OUT/'final-audit.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('linked_original_sources','current_untracked')},indent=2))

if __name__=='__main__':main()
