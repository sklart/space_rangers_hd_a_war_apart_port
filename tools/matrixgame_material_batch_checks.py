"""Short packet bytes/batch ownership regressions; saved Windows counts only."""
import argparse,csv,hashlib,json,subprocess
from pathlib import Path
from matrixgame_sdk_link_provenance import deko_link_provenance
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('windows','host','arm64','baseline'));kind=ap.parse_args().kind
    parent=ROOT/'build/matrixgame/e610-material-r6-checks'/kind;parent.mkdir(parents=True,exist_ok=True);index=1
    while (parent/str(index)).exists():index+=1
    out=parent/str(index);out.mkdir();j={'status':'INCOMPLETE','commands':[],'scope':'real packet bytes and CPU queue/ownership model; no GPU/Init; no Windows battle rerun'}
    captures=sorted((ROOT/'build/matrixgame/e2e539-batches/baseline-windows').glob('frame-cost-*.csv'));assert len(captures)==2
    rows=[]
    for p in captures:
        with p.open() as stream:rows+=list(csv.DictReader(stream))
    oracle={'frames':len(rows),'split_frames':sum(int(r['primary_draws'])>64 for r in rows),'peak_frame':max(int(r['primary_draws']) for r in rows),'submissions':sum((int(r['primary_draws'])+63)//64 for r in rows)}
    assert oracle=={'frames':92,'split_frames':92,'peak_frame':4038,'submissions':3310};j['capture_oracle']=oracle;j['captures_sha256']={str(p.relative_to(ROOT)):sha(p) for p in captures}
    oldmap=ROOT/'build/matrixgame/e610-staging-r5/1/hosted-diagnostic.map';deko_link_provenance(oldmap,'deko3d');rejected=False
    try:deko_link_provenance(oldmap,'deko3dd')
    except AssertionError:rejected=True
    assert rejected;j['R5_wrong_debug_provenance_rejected']=True
    def run(cmd,label,negative=False):
        p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'expected_failure':negative});print(kind,label,p.returncode,log[-2200:] if p.returncode or label.endswith('runtime') else '',flush=True)
        assert (p.returncode!=0 if negative else p.returncode==0),label;return log
    try:
        if kind=='baseline':
            data=subprocess.check_output(['C:/Program Files/Git/cmd/git.exe','show','937eccd13fc2b8911863459ae6e0ee734c31adca:port/matrixgame/native-engine/native_draw_batch.hpp']);(out/'r5-native_draw_batch.hpp').write_bytes(data);j['baseline_header_sha256']=sha(out/'r5-native_draw_batch.hpp')
        for name in ('material_uniform_packet','native_draw_batch'):
            if kind=='baseline' and name!='native_draw_batch':continue
            includes=[ROOT/'port/matrixgame/native-engine',ROOT/'port/matrixgame/native-core',ROOT/'port/matrixgame/deko',Path('C:/devkitPro/libnx/include'),out]
            source=ROOT/('tests/test_matrixgame_'+name+'.cpp');binary=out/name
            if kind=='windows':
                vs=r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
                command=f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG '+' '.join('/I"'+str(p)+'"' for p in includes)+f' "{source}" /Fo"{out/(name+".obj")}" /Fe"{binary}.exe"'
                script=out/(name+'.cmd');script.write_text(command+'\n');run(['cmd.exe','/d','/c',script],name+'-build');runtime=[str(binary)+'.exe']
            else:
                def wslpath(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
                flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
                if kind=='arm64':flags+=['-fsanitize=undefined','-fno-sanitize-recover=undefined']
                else:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
                if kind=='baseline':flags+=['-DMATRIXGAME_BATCH_BASELINE=1']
                run(['wsl','--exec','aarch64-linux-gnu-g++-15' if kind=='arm64' else 'g++',*flags,*['-I'+wslpath(p) for p in includes],linux(source),'-o',linux(binary)],name+'-build')
                runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
                if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
                runtime+=[linux(binary)]
            if name=='native_draw_batch':runtime+=list(map(str,captures)) if kind=='windows' else list(map(linux,captures))
            j[name+'_stdout']=run(runtime,name+'-runtime',kind=='baseline')
            if kind=='baseline':assert 'maximum_batch==64' in j[name+'_stdout']
        paths=['port/matrixgame/deko/material_uniform_packet.hpp','port/matrixgame/deko/material_program.cpp','port/matrixgame/native-engine/native_draw_batch.hpp','tests/test_matrixgame_material_uniform_packet.cpp','tests/test_matrixgame_native_draw_batch.cpp','tools/matrixgame_material_batch_checks.py','tools/matrixgame_sdk_link_provenance.py']
        j['source_sha256']={p:sha(ROOT/p) for p in paths};j['status']='PASS OLD 2048 BATCH POLICY REJECTED' if kind=='baseline' else 'PASS UNIFORM PACKET BYTES/64 DRAW BATCH/ORDER/RETIREMENT/FAILURE OWNERS; CPU ONLY'
    finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8');print('result',out/'result.json',flush=True)
if __name__=='__main__':main()
