"""Windows CPU resource contract; compile-check unchanged D3D bridge branches."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/residency'
def main():
    out=BASE/'windows';out.mkdir(parents=True,exist_ok=True)
    old=json.loads((ROOT/'build/matrixgame/e2e521/windows/result.json').read_text());report={'status':'INCOMPLETE','commands':[],'Switch_runtime':False}
    def run(command,label,cwd=ROOT):
        script=out/(label+'.cmd');script.write_text(command+'\n');p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace')
        report['commands'].append({'label':label,'command':command,'exit':p.returncode});(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        print(label,p.returncode,(p.stdout+p.stderr)[-5000:] if p.returncode else '',flush=True)
        if p.returncode:raise RuntimeError(label)
    sources=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_texture.cpp','platform_3g.cpp','legacy_texture_backend.cpp','unmanaged_texture_backend.cpp')]+[ROOT/'tests/test_matrixgame_texture_residency.cpp']
    try:
        objects=[]
        for source in sources:
            obj=out/(source.stem+'.obj');command=old['commands']['map-device'].replace(str(ROOT/'port/matrixgame/native-engine/map_device_backend.cpp'),str(source)).replace(str(ROOT/'build/matrixgame/e2e521/windows/map-device.obj'),str(obj))
            run(command,source.stem)
            if source.stem not in ('legacy_texture_backend','unmanaged_texture_backend'):objects.append(obj)
        legacy=ROOT/'build/matrixgame/e2e519/original-init-windows'
        setup=old['commands']['link'].split(' && link ')[0]
        libs=['core/Release/matrixgame-native-core.lib',
            '../windows/png/Release/libpng16_static.lib','../windows/zlib/Release/zs.lib','../windows/jpeg/Release/jpeg-static.lib']
        paths=[legacy/p for p in libs]
        command=setup+' && link /OPT:REF /INCREMENTAL:NO /OUT:"'+str(out/'texture-residency.exe')+'" '+' '.join('"'+str(p)+'"' for p in objects+paths)+' kernel32.lib'
        run(command,'link')
        inputs=json.loads((BASE/'host/result.json').read_text())['inputs'];files=[ROOT/n for n in inputs]
        p=subprocess.run([out/'texture-residency.exe',*files],cwd=ROOT,capture_output=True,text=True,timeout=90)
        report.update(runtime_exit=p.returncode,stdout=p.stdout,stderr=p.stderr);(out/'runtime.log').write_text(p.stdout+p.stderr)
        print('runtime',p.returncode,p.stdout,p.stderr,flush=True);assert p.returncode==0
        report['inputs']=inputs
        headers=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_texture.hpp','texture_residency.hpp','legacy_texture_backend.hpp')]
        report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+headers+[Path(__file__).resolve()]}
        report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
