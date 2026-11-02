"""Real original inline Reset with Win32 record -> Win64 SDK translation and ASan."""
import json,subprocess,shutil
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
from matrixgame_rangers_presentation_overrides import render_inline
BASE=ROOT/'build/matrixgame/e2e593-rangers-caller-reset/windows-asan-binding'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE)
 original=ROOT/'upstream/cpp/src/inline/Direct3D9.hpp';overlay=BASE/'source';overlay.mkdir(exist_ok=True)
 # Quoted includes of translated units require a self-contained header tree.
 prior=ROOT/'build/matrixgame/e2e577-rangers-windows/source'
 for source in prior.rglob('*.hpp'):
  dest=overlay/source.relative_to(prior);dest.parent.mkdir(parents=True,exist_ok=True)
  if dest.exists():assert sha(dest)==sha(source) if dest.relative_to(overlay).as_posix()!='inline/Direct3D9.hpp' else dest.read_text(encoding='utf-8')==render_inline(original.read_text(encoding='utf-8'))
  else:shutil.copyfile(source,dest)
 dest=overlay/'inline/Direct3D9.hpp';dest.write_text(render_inline(original.read_text(encoding='utf-8')),encoding='utf-8')
 sources=[ROOT/'tests/test_matrixgame_rangers_legacy_reset_windows.cpp',ROOT/'tests/matrixgame_rangers_legacy_reset_call.cpp',ROOT/'port/matrixgame/native-engine/rangers_legacy_presentation_win32.cpp']
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','FRun':'NOT RUN'}
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,env=env,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=BASE/(label+'.log');index=1
  while path.exists():path=BASE/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label;return log
 try:
  flags=['/nologo','/std:c++20','/EHsc','/MT','/O1','/UNDEBUG','/fsanitize=address','/D_DISABLE_VECTOR_ANNOTATION','/D_DISABLE_STRING_ANNOTATION','/I'+str(overlay),'/I'+str(ROOT/'port/matrixgame/native-engine')];objects=[]
  for source in sources:
   obj=BASE/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  binary=BASE/'legacy-reset.exe';run([compiler(env),'/nologo','/MT','/fsanitize=address',*objects,'d3d9.lib','user32.lib','/Fe'+str(binary)],'link')
  result['stdout']=run([binary],'runtime')
  assert 'original_inline_entry=PASS real_HAL_Reset=PASS SDK_in_out_copy=PASS unaligned_56byte_canaries=PASS' in result['stdout'] and 'actual_release_retry=PASS' in result['stdout']
  result.update(status='PASS; ORIGINAL RANGERS INLINE RESET WITH REAL WINDOWS X64 HAL AND ASAN; FRUN/GALAXY NOT RUN',flags=flags,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,original,ROOT/'port/matrixgame/native-engine/rangers_legacy_presentation.hpp',ROOT/'tools/matrixgame_rangers_presentation_overrides.py',ROOT/'tools/matrixgame_rangers_legacy_reset_windows.py']},generated_headers_sha256={str(p.relative_to(ROOT)):sha(p) for p in overlay.rglob('*.hpp')},objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary_sha256=sha(binary))
 finally:
  path=BASE/'result.json';index=1
  while path.exists():path=BASE/f'result-{index}.json';index+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
