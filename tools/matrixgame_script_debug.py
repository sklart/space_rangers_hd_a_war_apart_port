"""Bounded debugger reproduction of the failed host script probe; never a PASS gate."""
import json,subprocess
from matrixgame_standard_nro_audit import ROOT,sha
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
if __name__=='__main__':
 base=ROOT/'build/matrixgame/e2e595-script-session/asan';out=base/'debug';out.mkdir(exist_ok=True);i=1
 while (out/f'run-{i}.json').exists():i+=1
 binary=base/'government'
 command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1','timeout','--signal=INT','--kill-after=5s','12s','gdb','-q','-batch','-ex','set pagination off','-ex','set confirm off','-ex','start','-ex','break exit','-ex','continue','-ex','thread apply all bt','--args',linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(base/'runtime')]
 p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=40)
 (out/f'gdb-{i}.log').write_text(p.stdout+p.stderr,encoding='utf-8');(out/f'run-{i}.json').write_text(json.dumps({'status':'DEBUG ONLY; BOUNDED REPRODUCTION','command':command,'exit':p.returncode,'binary_sha256':sha(binary)},indent=2)+'\n',encoding='utf-8')
 print((p.stdout+p.stderr)[-15000:])
