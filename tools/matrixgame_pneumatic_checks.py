"""Execute the actual pneumatic release method with its original class and allocator."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
from matrixgame_vector_object_overrides import function
BASE=ROOT/'build/matrixgame/e2e542-pneumatic'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('baseline','host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True)
 path=ROOT/'upstream/matrixgame/MatrixGame/src/MatrixObjectRobot.cpp' if kind=='baseline' else BASE/'source/MatrixGame/src/MatrixObjectRobot.cpp'
 body=function(path.read_text(encoding='utf-8'),'void CMatrixRobot::DestroyPneumaticData(void)')
 module=out/'pneumatic-release.cpp';module.write_text((ROOT/'tests/test_matrixgame_pneumatic_release.cpp').read_text()+ '\n'+body+'\n',encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'method_source':str(path.relative_to(ROOT)),'method_source_sha256':sha(path),'method':body,'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 def run(cmd,label):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-started));print(label,p.returncode,log[-3500:] if p.returncode or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  return p.returncode
 try:
  profile='arm64' if kind=='arm64' else 'host';cmd=json.loads((ROOT/f'build/matrixgame/e2e541-resource-root/{profile}/result.json').read_text())['commands'][0]['command'][:]
  cmd=[s for s in cmd if not s.endswith('.cpp') and not s.startswith('-DMATRIXGAME_RESOURCE_ROOT_CANDIDATE=')]
  cmd=[s.replace('/build/matrixgame/e2e521/source/','/build/matrixgame/e2e542-pneumatic/source/') for s in cmd]
  at=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd.insert(at,linux(module));cmd[cmd.index('-o')+1]=linux(out/'pneumatic-release');assert run(cmd,'build')==0
  runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  code=run(runtime+[linux(out/'pneumatic-release')],'runtime')
  if kind=='baseline':assert code!=0 and 'double-free' in result['stdout'];result['status']='BASELINE REPRODUCED'
  else:assert code==0 and 'PNEUMATIC_RELEASE PASS' in result['stdout'];result['status']='PASS'
  result['binary_sha256']=sha(out/'pneumatic-release')
 finally:
  result['source_sha256']={n:sha(ROOT/n) for n in ('tools/matrixgame_actor_overrides.py','tools/matrixgame_pneumatic_checks.py','tests/test_matrixgame_pneumatic_release.cpp')};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
