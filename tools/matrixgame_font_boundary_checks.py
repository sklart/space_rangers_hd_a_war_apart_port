"""Address the next real map backend refusal without pretending to run native Init."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521/font'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=BASE/'boundary';out.mkdir(exist_ok=True)
    prior=json.loads((ROOT/'build/matrixgame/e2e521/residency/host/result.json').read_text())
    command=next(c['command'].copy() for c in prior['commands'] if c['label']=='build-link')
    sources=[ROOT/'port/matrixgame/native-engine/native_texture.cpp',ROOT/'port/matrixgame/native-engine/platform_3g.cpp',ROOT/'tests/test_matrixgame_map_bc1_boundary.cpp']
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    command=[s for s in command if not s.endswith('.cpp')]
    first=next(i for i,s in enumerate(command) if s.endswith('.a'));command[first:first]=[linux(p) for p in sources]
    command[command.index('-o')+1]=linux(out/'map-bc1-boundary')
    result={'status':'INCOMPLETE','commands':[],'Switch_runtime':False,'native_Init_run':False}
    try:
        for label,cmd in [('build',command),('runtime',['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',linux(out/'map-bc1-boundary'),linux(BASE/'map-first-source.png')])]:
            p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr)
            result['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-3000:],flush=True);assert p.returncode==0
            if label=='runtime':result['stdout']=p.stdout
        assert 'native texture formatted decode conversion' in result['stdout']
        result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+[Path(__file__).resolve()]}
        result['input_sha256']={str((BASE/'map-first-source.png').relative_to(ROOT)):sha(BASE/'map-first-source.png')}
        result['status']='CONFIRMED_UNSUPPORTED';result['texture_uploads']=0
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
