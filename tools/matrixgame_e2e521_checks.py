"""Native entry negative runtime tests; actual Switch provider compile/link audit."""
import json,os,shlex,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OLD=ROOT/'build/matrixgame/e2e519'
BASE=ROOT/'build/matrixgame/e2e521'

def main():
    kind=sys.argv[1];assert kind in ('host','arm64','sdk')
    source=BASE/'source';out=BASE/kind;out.mkdir(parents=True,exist_ok=True)
    report={'commands':[],'results':{},'runtime_executed':False}
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',
             DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
    def run(cmd,label,cwd=ROOT):
        p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True)
        report['commands'].append(cmd);report['results'][label]=p.returncode
        (out/(label+'.log')).write_text(p.stdout+p.stderr)
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        print(kind,label,p.returncode,(p.stdout+p.stderr)[-4500:] if p.returncode or label=='runtime' else '',flush=True)
        return p
    def runtime():
        prior=json.loads((OLD/('original-init-'+kind)/'result.json').read_text())
        cmd=[str(out/'native-entry')]
        if kind=='arm64':cmd=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']+cmd
        p=run(cmd,'runtime',Path(prior['commands'][-1]['cwd']))
        report.update(runtime_executed=True,stdout=p.stdout,stderr=p.stderr)
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
        if p.returncode:raise SystemExit(p.returncode)
    if '--runtime-only' in sys.argv:
        report.update(json.loads((out/'result.json').read_text()))
        assert report['results']['link']==0
        runtime();return
    units={'init':ROOT/'port/matrixgame/native-engine/init_runtime.cpp',
           'platform':ROOT/'port/matrixgame/native-engine/platform_3g.cpp',
           'owner':ROOT/'port/matrixgame/native-engine/original_game.cpp',
           'game':source/'MatrixGame/src/MatrixGame.cpp'}
    if kind=='sdk':
        template=json.loads((OLD/'skin-borrow-sdk/result.json').read_text())['commands'][0]
        previous=OLD/'skin-borrow-source'
        units['switch']=ROOT/'port/matrixgame/native-engine/platform_switch.cpp'
        units['driver']=ROOT/'tests/probe_matrixgame_native_entry.cpp'
        units['frame']=source/'MatrixGame/src/OriginalWindowsFrame.cpp'
        units['visibility']=source/'MatrixGame/src/MatrixVisiCalc.cpp'
        units['frame_clock']=ROOT/'port/matrixgame/native-engine/native_frame_clock.cpp'
    else:
        prior=json.loads((OLD/('original-init-'+kind)/'result.json').read_text())
        template=prior['commands'][0]['command'];previous=OLD/'original-init-source'
        units['driver']=ROOT/'tests/test_matrixgame_native_entry.cpp'
    objects=[]
    for name,path in units.items():
        cmd=[v.replace(str(previous).replace('\\','/'),str(source).replace('\\','/')).replace(str(previous),str(source)) for v in template]
        cmd=[v.replace(str(OLD/('vector-group-fresh-'+kind)/'core/source'),str(source/'portable-core')) for v in cmd]
        cmd[cmd.index('-c')+1]=str(path);obj=out/(name+'.o');cmd[cmd.index('-o')+1]=str(obj)
        cmd.append('-UNDEBUG')
        if kind=='sdk':cmd+=['-IC:/devkitPro/libnx/include']
        p=run(cmd,'compile_'+name)
        if p.returncode:raise SystemExit(p.returncode)
        objects.append(str(obj))
    if kind=='sdk':
        sdk=OLD/'text-fixed-sdk'
        extra=[OLD/'skin-borrow-sdk/callbacks.o',OLD/'skin-borrow-sdk/vo.o',
               ROOT/'build/matrixgame/e2e520/checks-sdk/map.o',ROOT/'build/matrixgame/e2e520/checks-sdk/prepare.o',
               OLD/'path-sdk-assertions/path.o']
        libs=[OLD/'original-init-sdk/libmatrixgame-native-cache.a',sdk/'libmatrixgame-native-vector-object.a',
              sdk/'libmatrixgame-native-navigation.a',sdk/'core/libmatrixgame-native-core.a']
        cmd=[template[0],'-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIE',
             '-specs=C:/devkitPro/libnx/switch.specs','-Wl,--gc-sections',*objects,*map(str,extra),*map(str,libs),
             '-LC:/devkitPro/libnx/lib','-LC:/devkitPro/portlibs/switch/lib','-ldeko3d','-lnx','-lpng','-lz','-ljpeg','-o',str(out/'native-entry.elf')]
        p=run(cmd,'link')
        if not p.returncode:
            p=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-u',str(out/'native-entry.elf')],'undefined')
            report['strong_undefined']=[v for v in p.stdout.splitlines() if ' U ' in v]
            assert not report['strong_undefined']
    else:
        link=next(x['command'] for x in prior['commands'] if '-o' in x['command'] and x['command'][x['command'].index('-o')+1].endswith('/original-game-init'))
        extra=[str(OLD/('skin-borrow-assertions-'+kind)/'callbacks.o')]
        cmd=[link[0],'-Wl,--gc-sections',*objects,*extra,'-o',str(out/'native-entry'),*link[link.index('-o')+2:]]
        if kind=='host':cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        p=run(cmd,'link',OLD/('vector-group-fresh-'+kind))
        if not p.returncode:
            runtime()
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    if p.returncode:raise SystemExit(p.returncode)

if __name__=='__main__':main()
