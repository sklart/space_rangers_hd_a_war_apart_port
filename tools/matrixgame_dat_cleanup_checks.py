"""Address/undefined sanitizer regression for both original DAT loader exits."""
import json
import subprocess
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
BASE=ROOT/'build/matrixgame/e2e573-rangers-data/dat-cleanup-asan'
def main():
 BASE.mkdir(parents=True,exist_ok=True);prior=read(ROOT/'build/matrixgame/e2e573-rangers-data/asan-final/result.json')
 objects=dict(prior['reused_objects_sha256']);objects.update({name:value for name,value in prior['new_objects_sha256'].items() if name.endswith(('EC_Data.o','EC_BlockPar.o'))});check_hashes(objects)
 source=ROOT/'tests/probe_matrixgame_dat_cleanup.cpp';before=sha(source);obj=BASE/'probe.o';result={'status':'INCOMPLETE','commands':[]}
 def run(command,label):
  p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (BASE/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode;return log
 try:
  command=next(c['command'] for c in prior['commands'] if c['label']=='compile-probe_matrixgame_rangers_data')[:]
  command[command.index('-c')+1]=linux(source);command[command.index('-o')+1]=linux(obj);run(command,'compile')
  manifest={'objects':[linux(obj)]+[linux(ROOT/name) for name in objects],'output':linux(BASE/'dat-cleanup'),'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(BASE/'link-report.json'),'link_flags':['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']}
  (BASE/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(BASE/'link-input.json')],'link')
  result['stdout']=run(['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',linux(BASE/'dat-cleanup'),linux(GAME),linux(BASE/'runtime')],'runtime')
  assert 'DAT_CLEANUP missing_open=2 short_read=2 exception=PASS handles_closed=PASS NO_FORMAT_CHANGES' in result['stdout'];check_hashes(objects);assert sha(source)==before
  result['reused_objects_sha256']=objects;result['probe_object_sha256']=sha(obj);result['binary_sha256']=sha(BASE/'dat-cleanup');result['status']='PASS; BOTH DAT LOADERS MISSING OPEN/SHORT READ; ASAN/UBSAN NEW TUS'
 finally:
  result['source_sha256']={str(source.relative_to(ROOT)):before};(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
