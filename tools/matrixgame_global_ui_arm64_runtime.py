"""Finish the slow QEMU gate using its unchanged successfully linked binary."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_galaxy_prerequisites_checks import latest,linux
BASE=ROOT/'build/matrixgame/e2e597-global-ui'
def main():
 failed=BASE/'arm64/1/result.json';prior=read(failed);assert prior['status']=='INCOMPLETE'
 assert [c['exit'] for c in prior['commands']]==[0,0,0,'TIMEOUT']
 live=subprocess.run(['wsl','--exec','pgrep','-a','-f','e2e597-global-ui/arm64'],capture_output=True,text=True)
 assert live.returncode==1 and not live.stdout,'previous QEMU process still alive or process query failed'
 windows=read(max((BASE/'windows-asan').glob('*/result.json'),key=lambda p:p.stat().st_mtime_ns))
 assert windows['status'].startswith('PASS');check_hashes(windows['sources_sha256'])
 dependency=latest(ROOT/'build/matrixgame/e2e595-script-session/arm64')
 reused={k:v for k,v in {**dependency['reused_objects_sha256'],**dependency['objects_sha256']}.items() if not k.replace('\\','/').endswith('/probe_matrixgame_script_session.o')};check_hashes(reused);check_hashes(dependency['libraries_sha256'])
 binary=BASE/'arm64/1/galaxy-prerequisites';objects=list((BASE/'arm64/1').glob('*.o'));assert len(objects)==2
 object_hashes={str(p.relative_to(ROOT)):sha(p) for p in objects};binary_hash=sha(binary)
 folder=BASE/'arm64';i=2
 while (folder/str(i)).exists():i+=1
 out=folder/str(i);out.mkdir();runtime=out/'runtime';runtime.mkdir()
 command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','timeout','--kill-after=5s','480s','qemu-aarch64','-L','/usr/aarch64-linux-gnu',linux(binary),linux(ROOT/'windows/Space Rangers HD A War Apart'),linux(runtime)]
 result={'status':'INCOMPLETE','commands':[],'campaign_generation':'NOT RUN','Switch':'NOT RUN','attempt':i,'compile_link_baseline':str(failed.relative_to(ROOT)),'baseline_sha256':sha(failed)}
 try:
  p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=500)
  log=p.stdout+p.stderr;(out/'runtime.log').write_text(log,encoding='utf-8');result['stdout']=log;result['commands'].append({'label':'runtime-unchanged-binary','command':command,'exit':p.returncode,'log':str((out/'runtime.log').relative_to(ROOT))});print(p.returncode,log[-12000:],flush=True);assert not p.returncode
  assert log.count('screens=36 controls=10826 script_templates=40 global_variables=32 planet_templates=207 stars=73 original_full_initializer=PASS duplicate_owner=REJECTED campaign_generation=NOT_RUN')==2
  assert 'original_partial_failure_rollback=PASS unregistered_foreign_screen=REJECTED' in log and log.count('original_finalize_reset=PASS omitted_owners_released=PASS no_save_load=PASS')==2
  assert sha(binary)==binary_hash;check_hashes(object_hashes);check_hashes(reused);check_hashes(windows['sources_sha256'])
  result.update(status='PASS; ARM64/QEMU ORIGINAL FULL GLOBAL UI/GAMEPLAY INITIALIZER; UNCHANGED BINARY; NO GENERATED CAMPAIGN',flags=dependency['flags'],new_TUs_instrumented=False,original_dependencies_instrumented=False,sources_sha256=windows['sources_sha256'],objects_sha256=object_hashes,reused_objects_sha256=reused,libraries_sha256=dependency['libraries_sha256'],binary=str(binary.relative_to(ROOT)),binary_sha256=binary_hash,runner_sha256=sha(Path(__file__).resolve()))
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
