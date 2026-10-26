"""Real CFile/config/map storage under an explicit input root; never touches resources."""
import argparse,json,subprocess,time,shutil,sys
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
sys.path.insert(0,str(ROOT/'tests'))
from probe_matrixgame_native_file import fixtures
BASE=ROOT/'build/matrixgame/e2e541-resource-root'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('baseline','host','arm64'));ap.add_argument('--file-contract',action='store_true');args=ap.parse_args();kind=args.kind
 assert not (args.file_contract and kind=='baseline')
 out=BASE/(kind+'-file' if args.file_contract else kind);out.mkdir(parents=True,exist_ok=True);foreign=out/'foreign-cwd';foreign.mkdir(exist_ok=True)
 unit=out/'unit-root';(unit/'DaTa').mkdir(parents=True,exist_ok=True);(unit/'Cfg').mkdir(exist_ok=True)
 disk,pkg=fixtures(out/'fixture');shutil.copyfile(pkg,unit/'DaTa/RoBots.pkG');(unit/'MUSIC').write_bytes(b'loose');(unit/'Cfg/Thing.CfG').write_bytes(b'root-file')
 result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 sources=['tests/test_matrixgame_native_file.cpp' if args.file_contract else 'tests/test_matrixgame_resource_root.cpp']
 if kind!='baseline':sources+=['port/matrixgame/native-core/file_native.cpp','port/matrixgame/native-core/resource_paths.cpp']
 def run(command,label):
  start=time.monotonic();p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,command)),exit=p.returncode,seconds=time.monotonic()-start));print(label,p.returncode,log[-3000:] if p.returncode or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  return p.returncode
 try:
  platform='arm64' if kind=='arm64' else 'host';prior=json.loads((ROOT/f'build/matrixgame/e2e524-text/{platform}/result.json').read_text());command=prior['commands'][0]['command']
  command=[s for s in command if not s.endswith('.cpp') and (not s.endswith('.a') or s.endswith('libmatrixgame-native-core.a'))]
  at=next(i for i,s in enumerate(command) if s.endswith('.a'));command[at:at]=['-I'+linux(ROOT/'port/switch/platform'),f'-DMATRIXGAME_RESOURCE_ROOT_CANDIDATE={int(kind!="baseline")}',*map(lambda p:linux(ROOT/p),sources)]
  if '-UNDEBUG' not in command:command.insert(3,'-UNDEBUG')
  command.insert(command.index('-o'),linux(ROOT/'build/e2e515/mg-navigation-arm64/zlib/libz.a') if kind=='arm64' else '-lz');command[command.index('-o')+1]=linux(out/'resource-root');assert run(command,'build')==0
  result['core_archive_sha256']={s:sha(Path(s.replace('/mnt/d/','D:/'))) for s in command if s.endswith('.a')}
  runtime=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':runtime+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  resource=ROOT/'build/matrixgame/e2e519/skin-borrow-windows/runtime';runtime_args=[linux(disk),linux(pkg)] if args.file_contract else [linux(resource),linux(foreign),linux(unit)];code=run(runtime+[linux(out/'resource-root'),*runtime_args],'runtime')
  if kind=='baseline':assert code==10 and 'source=CGame::Init->CFile::OpenPackFiles' in result['stdout'];result['status']='BASELINE REPRODUCED'
  else:assert code==0 and ('"file_io":"PASS"' in result['stdout'] if args.file_contract else 'RESOURCE_ROOT PASS' in result['stdout']);result['status']='PASS'
  result['binary_sha256']=sha(out/'resource-root')
 finally:
  result['source_sha256']={name:sha(ROOT/name) for name in sources+['tools/matrixgame_resource_checks.py']};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
