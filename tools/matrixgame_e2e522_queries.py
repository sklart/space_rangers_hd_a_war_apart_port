"""CPU native map query provider contract, distinct from physical GPU status."""
import argparse,json,hashlib,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e522'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind;out=BASE/('queries-'+kind);out.mkdir(exist_ok=True);report={'status':'INCOMPLETE','commands':[]}
    prior=json.loads((BASE/kind/'result.json').read_text());cmd=prior['commands'][0]['command'].copy();cmd=[s for s in cmd if not s.endswith('.cpp')]
    names=('port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/native-engine/platform_3g.cpp','port/matrixgame/native-engine/map_device_backend.cpp','port/matrixgame/native-engine/offscreen_backend.cpp','port/matrixgame/native-engine/transition_backend.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp','tests/test_matrixgame_native_map_queries.cpp');sources=[ROOT/n for n in names]
    def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
    first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=list(map(linux,sources));cmd[cmd.index('-o')+1]=linux(out/'queries')
    runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1']
    if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
    runtime+=[linux(out/'queries')]
    try:
        for label,command in [('build',cmd),('runtime',runtime)]:
            p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');report['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2500:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0
            if label=='runtime':report['stdout']=p.stdout
        report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+[ROOT/'port/matrixgame/native-engine/native_map_queries.hpp',ROOT/'port/matrixgame/native-engine/native_frame_scope.hpp',ROOT/'port/matrixgame/deko/fixed_vertex.hpp',Path(__file__).resolve()]};report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
