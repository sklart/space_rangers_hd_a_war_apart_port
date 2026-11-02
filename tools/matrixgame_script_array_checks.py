"""Exercise real VM array constructors/destructors and borrowed aliases with sanitizers."""
import json, subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_campaign_cleanup_checks import BASE, linux

def main():
 parent=BASE/'array-lifetime';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir()
 prior_dir=BASE/'asan/5';prior=read(prior_dir/'result.json');assert prior['status'].startswith('PASS')
 check_hashes(prior['generated_source_sha256']);check_hashes(prior['objects_sha256']);check_hashes(prior['reused_objects_sha256'])
 manifest=read(prior_dir/'link-input.json');source=ROOT/'tests/probe_matrixgame_script_array_lifetime.cpp';obj=out/'array_probe.o';binary=out/'array-lifetime'
 result={'status':'INCOMPLETE','commands':[],'campaign_runtime':'NOT REPEATED','source_sha256':sha(source),'prior_result_sha256':sha(prior_dir/'result.json'),'lifetime_header_sha256':sha(ROOT/'port/switch/platform/script_array_lifetime.hpp')}
 def run(command,label):
  p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=240)
  log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'command':list(map(str,command)),'exit':p.returncode,'label':label});print(label,p.returncode,log[-8000:],flush=True);assert p.returncode==0,label
  return log
 try:
  run(['wsl','--exec','g++',*prior['flags'],'-c',linux(source),'-o',linux(obj)],'compile')
  objects=[p for p in manifest['objects'] if Path(p).name!='campaign_probe.o'];assert len(objects)==len(manifest['objects'])-1
  manifest.update(objects=[*objects,linux(obj)],output=linux(binary),report=linux(out/'link-report.json'))
  path=out/'link-input.json';path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
  run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_government_cpu_link.py'),linux(path)],'link')
  result.update(binary_sha256=sha(binary),object_sha256=sha(obj),stdout=run(['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',linux(binary)],'runtime'))
  assert 'original_ctor_dtor=PASS' in result['stdout'] and 'cycles=2' in result['stdout']
  assert sha(source)==result['source_sha256'];check_hashes(prior['generated_source_sha256']);check_hashes(prior['objects_sha256']);check_hashes(prior['reused_objects_sha256'])
  result['status']='PASS; HOST ASAN/UBSAN/LSAN REAL VM ARRAY ALIASING AND LIFETIME'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
