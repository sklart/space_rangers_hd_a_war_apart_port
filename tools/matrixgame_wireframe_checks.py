"""Original optional wireframe dependency: focused GPU/state checks."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from matrixgame_line_checks import ROOT,sha,linux,read

BASE=ROOT/'build/matrixgame/e2e563-wireframe'


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('windows','baseline','host','arm64'));args=parser.parse_args();kind=args.kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[]}
    def run(cmd,label):
        if isinstance(cmd,str):
            script=out/(label+'.cmd');script.write_text(cmd+'\n');cmd=['cmd.exe','/d','/c',script]
        cmd=list(map(str,cmd));start=time.monotonic();p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-start})
        print(kind,label,p.returncode,log[-6000:] if p.returncode or label=='runtime' else '',flush=True)
        assert p.returncode==0,label
        return log
    try:
        if kind=='windows':
            template=(ROOT/'build/matrixgame/e2e533-form/windows/build-platform_win32.cmd').read_text().split(' /c ')[0]
            sources=['tests/matrixgame_wireframe_windows.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
            objects=[]
            for source in sources:
                obj=out/(Path(source).stem+'.obj');objects.append(obj)
                run(template+' /I"'+str(ROOT/'tests')+'" /I"'+str(ROOT/'port/matrixgame/deko')+'" /utf-8 /c "'+str(ROOT/source)+'" /Fo"'+str(obj)+'"','build-'+obj.stem)
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            exe=out/'wireframe-oracle.exe'
            run(f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && link /NOLOGO /OPT:REF /OUT:"{exe}" '+' '.join('"'+str(p)+'"' for p in objects)+' d3d9.lib user32.lib gdi32.lib opengl32.lib','link')
            report['stdout']=run([exe,ROOT/'port/matrixgame/deko/material.frag.glsl'],'runtime')
            report['binary_sha256']=sha(exe)
            report['status']='PASS; WINDOWS HAL VS PRODUCTION GLSL ONLY'
            report['source_sha256']={name:sha(ROOT/name) for name in sources+['tests/matrixgame_material_gl_reference.hpp','tests/matrixgame_fog_windows.cpp','port/matrixgame/deko/material.vert.glsl','port/matrixgame/deko/material.frag.glsl']}
        else:
            previous=ROOT/'build/matrixgame/e2e562-single-sample'/('arm64' if kind=='arm64' else 'baseline')/'result.json'
            cmd=read(previous)['commands'][0]['command'][:]
            old=linux(ROOT/'tests/test_matrixgame_single_sample.cpp')
            assert old in cmd;cmd[cmd.index(old)]=linux(ROOT/'tests/test_matrixgame_wireframe_state.cpp')
            exe=out/'wireframe-state';cmd[cmd.index('-o')+1]=linux(exe)
            if kind!='baseline':cmd[3:3]=['-DMATRIXGAME_WIREFRAME_CANDIDATE','-I'+linux(ROOT/'port/matrixgame/deko'),'-I/mnt/c/devkitPro/libnx/include']
            run(cmd,'build')
            runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            report['stdout']=run(runtime+[linux(exe)]+(['--baseline'] if kind=='baseline' else []),'runtime')
            names=['tests/test_matrixgame_wireframe_state.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-engine/native_device_state.hpp']
            if kind!='baseline':names+=['port/matrixgame/deko/legacy_polygon_mode.hpp']
            report['source_sha256']={name:sha(ROOT/name) for name in names}
            report['binary_sha256']=sha(exe);report['status']='PASS; BASELINE REPRODUCED' if kind=='baseline' else 'PASS; NATIVE STATE AND REAL SDK RASTERIZER FIELDS ONLY'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
