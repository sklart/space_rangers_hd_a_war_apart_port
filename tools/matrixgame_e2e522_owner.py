"""Execute original typed owners; never claims CPU upload model is a GPU."""
import argparse,json,subprocess,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e522'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind;out=BASE/('owner-'+kind);out.mkdir(exist_ok=True);report={'status':'INCOMPLETE','commands':[]}
    prior=json.loads((BASE/kind/'result.json').read_text());cmd=prior['commands'][0]['command'].copy();cmd=[s for s in cmd if not s.endswith('.cpp')]
    sources=[ROOT/n for n in ('port/matrixgame/native-core/texture_mips.cpp','port/matrixgame/native-engine/native_texture.cpp','port/matrixgame/native-engine/platform_3g.cpp','port/matrixgame/native-engine/legacy_texture_backend.cpp','build/matrixgame/e2e521/source/MatrixLib/3G/OriginalManagedTexture.cpp','build/matrixgame/e2e521/source/MatrixGame/src/MatrixMapTexture.cpp','tests/matrixgame_bc1_original_owner.cpp','tests/test_matrixgame_bc1_owner_main.cpp')]
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=list(map(linux,sources))+[linux(ROOT/('build/matrixgame/e2e519/'+('original-init-host' if kind=='host' else 'bottom-integration/arm64')+'/libmatrixgame-native-cache.a'))];first=next(i for i,s in enumerate(cmd) if s.endswith('libmatrixgame-native-core.a'));cmd[first:first]=[linux(ROOT/('build/matrixgame/e2e519/vector-group-fresh-'+kind+'/libmatrixgame-native-vector-object.a'))];cmd[cmd.index('-o')+1]=linux(out/'original-owner')
    runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1']
    if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
    runtime+=[linux(out/'original-owner')]
    try:
        for label,command in [('build',cmd),('runtime',runtime)]:
            p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');report['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-3500:],flush=True);assert p.returncode==0
            if label=='runtime':report['stdout']=p.stdout
        report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[Path(__file__).resolve()]};report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
