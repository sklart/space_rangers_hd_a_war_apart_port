"""Addressed regression of changed texture/mip contracts; not the old GPU matrix."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e522/compat';PY=Path(__file__).resolve()
def main():
    OUT.mkdir(exist_ok=True);report={'status':'INCOMPLETE','commands':[]}
    prior=json.loads((ROOT/'build/matrixgame/e2e521/residency/host/result.json').read_text());template=prior['commands'][0]['command'].copy()
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    def run(cmd,label,binary=False):
        p=subprocess.run(cmd,cwd=ROOT,capture_output=True);(OUT/(label+'.stdout')).write_bytes(p.stdout);(OUT/(label+'.stderr')).write_bytes(p.stderr);report['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(label,p.returncode,p.stderr.decode('utf-8',errors='replace')[-2000:] if p.returncode else '',flush=True);assert p.returncode==0,label;return p.stdout
    try:
        baseline=OUT/'baseline-mips.cpp';data=subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe','show','HEAD:port/matrixgame/native-core/texture_mips.cpp'],cwd=ROOT);baseline.write_bytes(data);report['baseline_source_sha256']=hashlib.sha256(data).hexdigest()
        sources=[ROOT/'port/matrixgame/native-core/texture_mips.cpp',ROOT/'tests/test_matrixgame_native_mips.cpp']
        manifest=OUT/'inputs.txt';inputs=[ROOT/'build/matrixgame/e2e521/residency/host/inputs'/n for n in ('rgba.dds','bc1.dds','bc2.dds','bc3.dds','original.dds','original.png','original.jpg','original.strg')]+[ROOT/'build/matrixgame/e2e522/windows-frame/union-0.dds',ROOT/'build/matrixgame/e2e522/host/union-0-native.dds'];manifest.write_bytes(('\n'.join(map(linux,inputs))+'\n').encode('utf-8'))
        cmd=[s for s in template if not s.endswith('.cpp')];first=next(i for i,s in enumerate(cmd) if s.endswith('.a'))
        for name,mips in [('baseline',baseline),('candidate',sources[0])]:
            command=cmd.copy();command[first:first]=[linux(mips),linux(sources[1])];command[command.index('-o')+1]=linux(OUT/name);run(command,'build-'+name)
        for policy in ('complete','single'):
            outputs=[]
            for name in ('baseline','candidate'):
                runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',linux(OUT/name)]+(['--single'] if policy=='single' else [])+[linux(manifest)];outputs.append(run(runtime,name+'-'+policy,True))
            assert outputs[0]==outputs[1],policy
        command=template.copy();first=next(i for i,s in enumerate(command) if s.endswith('.a'));command.insert(first,linux(sources[0]));command[command.index('-o')+1]=linux(OUT/'residency');run(command,'build-residency')
        runtime=prior['commands'][1]['command'].copy();runtime[runtime.index(next(s for s in runtime if s.endswith('/texture-residency')))]=linux(OUT/'residency');report['residency_stdout']=run(runtime,'residency').decode('utf-8')
        report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[ROOT/'port/matrixgame/native-engine/native_texture.cpp',ROOT/'tests/test_matrixgame_texture_residency.cpp',PY]};report['status']='PASS';print('mip baseline/candidate byte equality for 10 inputs x2 policies + residency PASS',flush=True)
    finally:(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
