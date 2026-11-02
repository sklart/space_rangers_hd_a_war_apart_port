"""Addressed real filesystem/durable diagnostic journal checks. No NRO."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e543-journal'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));ap.add_argument('--output',type=Path,default=BASE);args=ap.parse_args();kind=args.kind
 out=args.output.resolve()/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 sources=['tests/test_matrixgame_diagnostic_journal.cpp','port/matrixgame/native-engine/native_diagnostic_journal.cpp']
 def run(cmd,label):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-started));print(label,p.returncode,log[-4000:] if p.returncode or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  assert p.returncode==0,label
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-pthread','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'journal-test';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),*[linux(ROOT/p) for p in sources],'-o',linux(exe)],'build')
  cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  before=set((out/'filesystem').rglob('run.jsonl'))
  run(cmd+[linux(exe),linux(out/'filesystem')],'runtime')
  logs={}
  for path in (out/'filesystem').rglob('run.jsonl'):
   if path in before:continue
   try:records=[json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
   except (UnicodeError,json.JSONDecodeError):continue # Deliberate short writes leave a partial last record.
   if any(r['operation']=='max_uint64' for r in records):
    assert len([r for r in records if r['operation']=='worker_counter'])==200
    assert next(r for r in records if r['operation']=='max_uint64')['value']==2**64-1
    assert next(r for r in records if r['operation']=='utf8_detail')['detail']==('a'*767)
    assert next(r for r in records if r['operation']=='utf8_detail')['detail_truncated']
    failure=next(r for r in records if r['event']=='failed' and r['operation']=='stor.Load');assert failure['last_ok']=='RobotPreload' and failure['result'] is None
    assert next(r for r in records if r['operation']=='ResultOperation' and r['event']=='failed')['result']==0xdead
    sdk=next(r for r in records if r['event']=='sdk_error');assert sdk['operation']=='deko3d.fixture' and sdk['result']==3
    assert all(r['run_id']==records[0]['run_id'] and r['source_sha256']==records[0]['source_sha256'] for r in records)
    assert [r['seq'] for r in records]==list(range(1,len(records)+1));logs[str(path.relative_to(ROOT))]=sha(path)
  assert logs,'no complete journal oracle';result.update(status='PASS',binary_sha256=sha(exe),verified_journals_sha256=logs)
 finally:
  result['source_sha256']={p:sha(ROOT/p) for p in sources+['port/matrixgame/native-engine/native_diagnostic_journal.hpp','port/matrixgame/native-engine/native_diagnostic_control.hpp','tools/matrixgame_journal_checks.py']};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
