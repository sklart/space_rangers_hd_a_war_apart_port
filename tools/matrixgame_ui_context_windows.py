"""Real read-only CFG/TCC/software canvas/UI ownership on the Windows reference."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler

def main():
 parent=ROOT/'build/matrixgame/e2e607-ui-context/windows';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='windows')
 old=ROOT/'build/matrixgame/e2e600-campaign-navigation/snapshot-windows/4';prior=read(old/'result.json');assert prior['status'].startswith('PASS');check_hashes(prior['objects_sha256']);check_hashes(prior['reused_inputs_sha256'])
 flags=read(ROOT/'build/matrixgame/e2e599-campaign-cleanup/windows/1/result.json')['flags']+['/I'+str(ROOT/'port/matrixgame/native')]
 sources=[ROOT/'tests/probe_matrixgame_ui_context.cpp',ROOT/'port/matrixgame/native/rangers_ui_context.cpp',ROOT/'port/switch/platform/renderer_platform.cpp'];result={'status':'INCOMPLETE','commands':[],'scope':'WINDOWS CPU ORIGINAL UI CONTEXT; NO GPU OR BATTLE','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_ui_context.hpp',Path(__file__).resolve()]},'flags':flags}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600);log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-6500:] if p.returncode else '\n'.join(l for l in log.splitlines() if l.startswith('UI_CONTEXT')),flush=True)
  if label=='runtime':result['stdout']=log
  assert p.returncode==0,label
 try:
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  inputs=[Path(line.strip('"')) for line in (old/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name not in ('navigation_probe.obj','renderer_platform.obj')]
  hashes={str(p):sha(p) for p in inputs};rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*inputs,*objects]),encoding='utf-8');binary=out/'ui-context.exe';command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:];command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];run(command,'link')
  runtime=out/'runtime';runtime.mkdir();env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH'];run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],'runtime')
  assert result['stdout'].count('original_CFG_TCC_canvas_palette_cursors=PASS original_script_global_UI=PASS')==2 and 'cleanup=PASS' in result['stdout'];check_hashes(hashes);check_hashes(result['sources_sha256'])
  result.update(status='PASS WINDOWS ASAN ORIGINAL CFG/TCC/CANVAS/DEPTH/PALETTE/CURSORS/SCRIPT/GLOBAL UI TWO CYCLES/CLEANUP; PARTIAL INSTRUMENTATION',reused_inputs_sha256=hashes,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary_sha256=sha(binary),new_NRO=False,Switch='NOT RUN')
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
