"""Real original Init/form/L3GRun/input/Draw/Deinit on Windows D3D9 HAL."""
import hashlib,json,os,subprocess,sys,uuid
from pathlib import Path
from matrixgame_form_overrides import KEYS
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e533-form/windows'
sys.stdout.reconfigure(encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 OUT.mkdir(parents=True,exist_ok=True);source=ROOT/'build/matrixgame/e2e533-form/sdk/source'
 prior=json.loads((ROOT/'build/matrixgame/e2e528-minimap/windows-frame/result.json').read_text())
 j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN'}
 def run(command,label,cwd=ROOT):
  if isinstance(command,str):
   script=OUT/(label+'.cmd');script.write_text(command+'\n');cmd=['cmd.exe','/d','/c',script]
  else:cmd=command
  p=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace',env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),timeout=240)
  (OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode})
  print(label,p.returncode,(p.stdout+p.stderr)[-6500:] if p.returncode or label=='runtime' else '',flush=True)
  if p.returncode:raise RuntimeError(label+' failed')
  return p.stdout
 try:
  outputs=OUT/('private-output-'+uuid.uuid4().hex);harness=OUT/'driver.cpp'
  harness.write_text('#define MATRIXGAME_FORM_TEST_OUTPUT L"'+outputs.as_posix()+'"\n'+(ROOT/'tests/matrixgame_windows_form_loop.cpp').read_text())
  # Compare every numeric key used by the adapter against original input.hpp
  # and the installed official Windows SDK at compile time.
  keys=OUT/'keys.cpp';keys.write_text('#include "'+(ROOT/'upstream/matrixgame/MatrixGame/src/input.hpp').as_posix()+'"\n'+''.join(f'static_assert({n}=={v});\n' for n,v in sorted(KEYS.items())))
  template=prior['commands'][0]['command'].split(' /c ')[0]
  paths=[source/'MatrixGame/src/OriginalNativeForm.cpp',source/'MatrixLib/3G/OriginalNativeLoop.cpp',source/'MatrixLib/3G/DeviceState.cpp']+[ROOT/'port/matrixgame/native-engine'/n for n in ('native_form_backend.cpp','native_form_session.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp','platform_win32.cpp')]+[ROOT/'tests/matrixgame_windows_frame_main.cpp',ROOT/'tests/matrixgame_windows_stored_slots.cpp',harness,keys]
  objects=[]
  for path in paths:
   obj=OUT/(path.stem+'.obj');run(template+' /c "'+str(path)+'" /Fo"'+str(obj)+'"','build-'+path.stem);objects.append(obj)
  link=next(c['command'] for c in prior['commands'] if c['label']=='link');old=ROOT/'build/matrixgame/e2e528-minimap/windows-frame/matrixgame_windows_frame_init_output.obj';assert str(old) in link
  for old_object in (ROOT/'build/matrixgame/e2e521/windows/platform.obj',ROOT/'build/matrixgame/e2e521/windows/sdk-main.obj'):
   assert str(old_object) in link;link=link.replace('"'+str(old_object)+'"','')
  link=link.replace('"'+str(old)+'"','');link=link.replace(str(ROOT/'build/matrixgame/e2e528-minimap/windows-frame/windows-game.exe'),str(OUT/'form-loop.exe')).replace(str(ROOT/'build/matrixgame/e2e528-minimap/windows-frame/windows-game.map'),str(OUT/'form-loop.map'))
  link+=' '+' '.join('"'+str(p)+'"' for p in objects);run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
  cwd=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text())['cwd'];stdout=run([str(OUT/'form-loop.exe'),str(OUT)],'runtime',cwd)
  assert stdout.count('REAL_FORM_LOOP draws=8 elapsed=125 Enter/Pause/Escape/scroll/MouseMove/F9_guard=PASS')==2
  j.update(status='PASS',stdout=stdout,actual_sessions=2,original_loop_form_draws=16,binary_sha256=sha(OUT/'form-loop.exe'),source_sha256={str(p.relative_to(ROOT)):sha(p) for p in paths+[ROOT/'tests/matrixgame_windows_form_loop.cpp',Path(__file__).resolve()]},output_root=str(outputs))
 finally:(OUT/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
