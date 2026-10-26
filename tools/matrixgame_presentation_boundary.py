"""Execute the next original text boundary without fabricating its provider."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e523-presentation/text-boundary'
def main():
    OUT.mkdir(parents=True,exist_ok=True);j=json.loads((ROOT/'build/matrixgame/e2e522/unions-host/result.json').read_text());cmd=[s for s in j['commands'][0]['command'] if not s.endswith('.cpp')]
    sources=[ROOT/n for n in ('tests/test_matrixgame_native_text_boundary.cpp','port/matrixgame/native-engine/platform_3g.cpp','port/matrixgame/native-engine/text_backend.cpp','build/matrixgame/e2e523-presentation/sdk/source/MatrixGame/src/Text/Render.cpp','build/matrixgame/e2e523-presentation/sdk/source/MatrixGame/src/Text/Font.cpp','build/matrixgame/e2e523-presentation/sdk/source/MatrixGame/src/Text/Parser.cpp')]
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=list(map(linux,sources));cmd[cmd.index('-o')+1]=linux(OUT/'boundary');result={'status':'INCOMPLETE','commands':[]}
    try:
        for label,command in [('build',cmd),('runtime',['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1',linux(OUT/'boundary')])]:
            p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2500:],flush=True);assert p.returncode==0
            if label=='runtime':result['stdout']=p.stdout
        result['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[Path(__file__).resolve()]};result['status']='PASS'
    finally:(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
