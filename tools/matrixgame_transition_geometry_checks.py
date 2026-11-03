"""Reproduce empty native-resolution transition and run original Takt on CFG data."""
import argparse,csv,hashlib,json,re,struct,subprocess
from pathlib import Path
from matrixgame_transition_overrides import prepare_transition
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def function(text,start):
    a=text.index(start);b=text.index('{',a)+1;depth=1
    while depth:
        if text[b]=='{':depth+=1
        elif text[b]=='}':depth-=1
        b+=1
    return text[a:b]
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def layouts(path):
    raw=path.read_bytes();text=raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig');a=text.index('Transitions {');data=function(text[a:],'Transitions')
    result={}
    for w,h,body in re.findall(r'(?ms)^\s*(\d+)\.(\d+)\s*\{(.*?)^\s*\}',data):
        geometry=[]
        for line in body.splitlines():
            line=line.strip()
            if not line:continue
            name,coords=line.split('=');direction,speed=name.split(',');dx,dy={'Up':(0,-1),'Down':(0,1),'Left':(-1,0),'Right':(1,0)}[direction];speed=f32(float(speed))
            points=[tuple(f32(float(v)) for v in point.split(',')) for point in coords.split('|')];assert len(points)==4
            geometry.append({'dir':(f32(dx*speed),f32(dy*speed)),'points':[points[i] for i in (0,1,3,2)]})
        result[int(w),int(h)]=geometry
    assert set(result)=={(1024,768),(800,600)} and all(result.values());return result
def oracle(layouts):
    rows=[]
    for w,h in ((1024,768),(800,600),(640,480),(1280,720),(1920,1080),(1023,777)):
        sw,sh=(w,h) if (w,h) in layouts else (1024,768);sx,sy=f32(w/sw),f32(h/sh)
        values=[{'pos':[0.,0.],'dir':[f32(g['dir'][0]*sx),f32(g['dir'][1]*sy)],'points':[(f32(x*sx),f32(y*sy)) for x,y in g['points']]} for g in layouts[sw,sh]]
        tick=0;rows.append([w,h,tick,len(values),1])
        while values and tick<500:
            tick+=1;i=0
            while i<len(values):
                g=values[i];g['pos']=[f32(g['pos'][a]+f32(g['dir'][a]*50.)) for a in (0,1)]
                if not any(0<=f32(x+g['pos'][0])<w and 0<=f32(y+g['pos'][1])<h for x,y in g['points']):values[i]=values[-1];values.pop()
                else:i+=1
            rows.append([w,h,tick,len(values),int(bool(values))])
        assert not values
    return rows
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64','windows'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-transition-r10-checks'/kind;parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();result={'status':'INCOMPLETE','commands':[],'scope':'authored resource geometry, original Takt/Clear, independent float32 oracle; CPU ONLY NO GPU'}
    upstream=ROOT/'upstream/matrixgame/MatrixGame/src/MatrixTransition.cpp';cfg=ROOT/'upstream/matrixgame/MatrixGame/CFG/robots/data.txt'
    data=layouts(cfg);fixture=out/'geometry.txt';fixture.write_text(str(len(data))+'\n'+''.join(f'{w} {h} {len(values)}\n'+''.join(' '.join(format(v,'.9g') for v in (*g['dir'],*[n for p in g['points'] for n in p]))+'\n' for g in values) for (w,h),values in data.items()),encoding='utf-8')
    generated=out/'source/MatrixGame/src';generated.mkdir(parents=True);(generated/'MatrixTransition.cpp').write_bytes(upstream.read_bytes());prepare_transition(out/'source');candidate=(generated/'OriginalTransition.cpp').read_text(encoding='utf-8');original=upstream.read_text(encoding='utf-8')
    takt=function(candidate,'void CTransition::Takt(int ms)');assert takt==function(original,'void CTransition::Takt(int ms)')
    (out/'original-takt.inc').write_text(takt+'\n',encoding='utf-8');(out/'candidate-clear.inc').write_text(function(candidate,'void CTransition::Clear(void)')+'\n',encoding='utf-8');(out/'baseline-clear.inc').write_text(function(original,'void CTransition::Clear(void)').replace('::Clear(','::OriginalClear(')+'\n',encoding='utf-8')
    assert candidate.count('* geometry.x_scale;')==4 and candidate.count('* geometry.y_scale;')==4
    assert 'm_Geom[i].dir.x *= geometry.x_scale;' in candidate and 'm_Geom[i].dir.y *= geometry.y_scale;' in candidate
    # Real capture, texture creation/upload and original animation remain present.
    for token in ('CaptureLoadingImage(bm, m_ScreenX, m_ScreenY)','m_Tex->LoadFromBitmap(bmout, 0x31545844u, 1)','RESETFLAG(g_MatrixMap->m_Flags, MMFLAG_TRANSITION)'):
        assert token in candidate,token
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(kind,label,p.returncode,log[-3000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label;return log
    try:
        source=ROOT/'tests/test_matrixgame_transition_geometry.cpp';exe=out/'transition';trace=out/'trace.csv';includes=[out,ROOT/'port/matrixgame/native-engine']
        if kind=='windows':
            vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat';cmd=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG /fp:strict '+' '.join('/I"'+str(p)+'"' for p in includes)+f' "{source}" /Fo"{out.as_posix()}/" /Fe"{exe}.exe"'
            script=out/'build.cmd';script.write_text(cmd+'\n');run(['cmd.exe','/d','/c',script],'build');runtime=[str(exe)+'.exe',fixture,trace]
        else:
            flags=['-std=c++20','-O2','-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined' if kind=='host' else '-fsanitize=undefined']
            run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],linux(source),'-o',linux(exe)],'build')
            runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
            runtime+=[linux(exe),linux(fixture),linux(trace)]
        result['stdout']=run(runtime,'runtime')
        with trace.open() as stream:rows=[[int(v) for v in row] for row in list(csv.reader(stream))[1:]]
        assert rows==oracle(data),'independent original-geometry float32 oracle mismatch'
        result['completion_ticks']={str((w,h)):max(r[2] for r in rows if r[:2]==[w,h]) for w,h in data.keys()|{(640,480),(1280,720),(1920,1080),(1023,777)}}
        result['trace_sha256']=sha(trace);result['status']='PASS AUTHORED TRANSITION SCALE/ORIGINAL TAKT/INDEPENDENT ORACLE/PARTIAL CLEANUP; CPU ONLY'
    finally:
        paths=['tests/test_matrixgame_transition_geometry.cpp','port/matrixgame/native-engine/transition_geometry.hpp','tools/matrixgame_transition_overrides.py','tools/matrixgame_transition_geometry_checks.py'];result['source_sha256']={p:sha(ROOT/p) for p in paths};result['upstream_cpp_sha256']=sha(upstream);result['cfg_sha256']=sha(cfg);result['generated_transition_sha256']=sha(generated/'OriginalTransition.cpp');(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
