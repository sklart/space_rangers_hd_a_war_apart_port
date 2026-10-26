"""Measure original primary-target draws per form frame. Never executes native GPU."""
import hashlib,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e539-batches'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 out=BASE/'baseline-windows';out.mkdir(parents=True,exist_ok=True)
 previous=ROOT/'build/matrixgame/e2e538-selection/map-state/windows'
 result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 sources=[]
 def run(command,label,cwd=ROOT):
  if isinstance(command,str):
   script=out/(label+'.cmd');script.write_text(command+'\n');command=['cmd.exe','/d','/c',script]
  start=time.monotonic();p=subprocess.run(list(map(str,command)),cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',env=dict(os.environ,MATRIXGAME_DRAW_AUDIT_OUTPUT=str(out),MATRIXGAME_UNION_OUTPUT=str(out)),timeout=360)
  log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,command)),exit=p.returncode,seconds=time.monotonic()-start))
  print(label,p.returncode,log[-3500:] if label=='runtime' or p.returncode else '',flush=True)
  if label=='runtime':result['stdout']=log
  return p.returncode
 try:
  template=(previous/'build-driver.cmd').read_text().split(' /c ')[0]
  driver=(previous/'driver.cpp').read_text();driver=driver.replace('#include "matrixgame_actual_draw_audit.hpp"','#include "matrixgame_actual_draw_audit.hpp"\n#include "matrixgame_draw_budget.hpp"')
  old='form.Draw();++draws;';assert driver.count(old)==1;driver=driver.replace(old,'{matrixgame_draw_audit::FrameBudget budget;form.Draw();}++draws;')
  # A new private tree for original minimap cache writes, never an old fixture.
  driver=driver.replace(str(previous.as_posix())+'/private-output-',out.as_posix()+'/private-output-')
  (out/'driver.cpp').write_text(driver,encoding='utf-8')
  selection=(ROOT/'tests/matrixgame_windows_selection.cpp').read_text();old='form.Draw();++frames;';assert selection.count(old)==1
  selection='#include "matrixgame_draw_budget.hpp"\n'+selection.replace(old,'{matrixgame_draw_audit::FrameBudget budget;form.Draw();}++frames;')
  (out/'matrixgame_windows_selection.cpp').write_text(selection,encoding='utf-8')
  sources=[out/'driver.cpp',out/'matrixgame_windows_selection.cpp',ROOT/'tests/matrixgame_draw_budget_windows.cpp']
  link=(previous/'link.cmd').read_text().strip().replace(str(previous/'actual-draw.exe'),str(out/'frame-budget.exe')).replace(str(previous/'actual-draw.map'),str(out/'frame-budget.map'))
  for source,oldstem in zip(sources,('driver','matrixgame_windows_selection','matrixgame_actual_draw_windows')):
   obj=out/(source.stem+'.obj');assert run(template+' /c "'+str(source)+'" /Fo"'+str(obj)+'"','build-'+source.stem)==0
   token='"'+str(previous/(oldstem+'.obj'))+'"';assert link.count(token)==1;link=link.replace(token,'"'+str(obj)+'"')
  assert run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')==0
  result['binary_sha256']=sha(out/'frame-budget.exe')
  result['baseline_recorder_sha256']=sha(ROOT/'port/matrixgame/native-engine/platform_switch.cpp')
  assert 'if(packets.size()>=2048)throw' in (ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text()
  (out/'platform_switch.cpp').write_bytes((ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_bytes())
  cwd=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text())['cwd']
  code=run([out/'frame-budget.exe',out],'runtime',cwd)
  result['status']='BASELINE SOURCE_POLICY_REJECT' if code==10 and 'original_HAL=0 native_2048_guard_rejected_sessions=2' in result['stdout'] else 'PASS' if code==0 else 'FAIL'
  assert result['status']!='FAIL'
 finally:
  result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+[ROOT/'tests/matrixgame_draw_budget.hpp',ROOT/'tests/matrixgame_draw_budget_windows.cpp',Path(__file__).resolve()]}
  result['capture_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.glob('frame-cost-*.csv')}
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
