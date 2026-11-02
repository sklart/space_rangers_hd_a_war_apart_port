"""Original native Init error boundary with a real owning RangersHostSession."""
import json
import subprocess
import shutil
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
BASE=ROOT/'build/matrixgame/e2e574-rangers-host-session/native-prefix-final'
def local(path):return Path(path.replace('/mnt/d/','D:/'))
def main():
 BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 prior=read(ROOT/'build/matrixgame/e2e568-native-services/host/result.json');check_hashes(prior['source_sha256'])
 native_root=BASE/'native-input';fixture={}
 for name in ('data.txt','iface.txt'):
  source_cfg=ROOT/'upstream/matrixgame/MatrixGame/CFG/robots'/name;dest=native_root/'CFG/robots'/name;dest.parent.mkdir(parents=True,exist_ok=True)
  if not dest.exists():shutil.copyfile(source_cfg,dest)
  assert sha(source_cfg)==sha(dest);fixture[str(dest.relative_to(ROOT))]=sha(dest)
 package=GAME/'DATA/robots.pkg';fixture[str(package.relative_to(ROOT))]=sha(package)
 package_link=native_root/'DATA/robots.pkg';package_link.parent.mkdir(parents=True,exist_ok=True)
 symlink_code='import pathlib,sys; target=pathlib.Path(sys.argv[1]); link=pathlib.Path(sys.argv[2]); link.symlink_to(target) if not link.is_symlink() and not link.exists() else None; assert link.is_symlink() and link.resolve()==target.resolve()'
 subprocess.run(['wsl','--exec','python3','-c',symlink_code,linux(package),linux(package_link)],check=True)
 host=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/asan/result.json');check_hashes(host['source_sha256'])
 objects={name:value for name,value in host['reused_objects_sha256'].items() if not name.endswith('package.o')}
 objects.update({name:value for name,value in host['new_objects_sha256'].items() if not name.endswith('probe_matrixgame_rangers_host_session.o')})
 link=next(c['command'] for c in prior['commands'] if c['label']=='link')
 native_objects=[local(name) for name in link if name.endswith('.o') and not name.endswith('test_matrixgame_native_services.o')]
 objects.update({str(path.relative_to(ROOT)):sha(path) for path in native_objects});check_hashes(objects)
 archives={str(local(name).relative_to(ROOT)):digest for name,digest in prior['archive_sha256'].items()};check_hashes(archives)
 source=ROOT/'tests/probe_matrixgame_hosted_native_prefix.cpp';before={str(source.relative_to(ROOT)):sha(source),str(Path(__file__).resolve().relative_to(ROOT)):sha(Path(__file__).resolve()),'tools/matrixgame_rangers_sound_link.py':sha(ROOT/'tools/matrixgame_rangers_sound_link.py')}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (BASE/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-8000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label;return log
 try:
  command=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime')[:];obj=BASE/'probe.o'
  command[command.index('-c')+1]=linux(source);command[command.index('-o')+1]=linux(obj)
  command += ['-I'+linux(ROOT/'port/matrixgame/native'),'-I'+linux(ROOT/'upstream/cpp/src'),'-I'+linux(ROOT/'upstream/cpp/runtime'),'-I'+linux(ROOT/'port/switch/platform')]
  run(command,'compile')
  manifest={'objects':[linux(obj)]+[linux(ROOT/name) for name in objects],'extra_archives':[linux(ROOT/name) for name in archives],'output':linux(BASE/'hosted-native-prefix'),'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(BASE/'link-report.json'),'link_flags':['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']}
  (BASE/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(BASE/'link-input.json')],'link')
  result['stdout']=run(['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1',linux(BASE/'hosted-native-prefix'),linux(GAME),linux(BASE/'runtime'),linux(native_root)],'runtime')
  assert 'original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS' in result['stdout']
  check_hashes(objects);check_hashes(archives);check_hashes(before);check_hashes(host['protected_game_sha256']);check_hashes(fixture)
  result['objects_sha256']=objects;result['archives_sha256']=archives;result['probe_object_sha256']=sha(obj);result['binary_sha256']=sha(BASE/'hosted-native-prefix')
  result['native_read_root']=str(native_root);result['native_fixture_sha256']=fixture
  result['status']='PASS; REAL HOST THROUGH ORIGINAL NATIVE INIT CAPS BOUNDARY/ROLLBACK; NO FULL INIT'
 finally:
  result['source_sha256']=before;(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
