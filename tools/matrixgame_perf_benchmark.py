"""Measure real host journal IO on the saved r7 event sequence, never Switch FPS."""
import hashlib,json,re,statistics,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    parent=ROOT/'build/matrixgame/e610-perf-r8-checks/benchmark';parent.mkdir(parents=True,exist_ok=True);i=1
    while (parent/str(i)).exists():i+=1
    out=parent/str(i);out.mkdir();result={'status':'INCOMPLETE','scope':'real host filesystem fflush/fsync, saved r7 event sequence; not microSD/GPU/FPS','commands':[]}
    capture=ROOT/'build/matrixgame/e610-r7-hardware-20261103/run-1793703659005706-0.jsonl'
    rows=[json.loads(l) for l in capture.read_bytes().splitlines()];assert len(rows)==7747
    tsv=out/'interactive.tsv';tsv.write_bytes(''.join(r['event']+'\t'+r['operation']+'\n' for r in rows).encode())
    files=['tests/benchmark_matrixgame_journal_replay.cpp','port/matrixgame/native-engine/native_diagnostic_journal.cpp']
    def run(cmd,label):
        cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(label,p.returncode,log[-2000:],flush=True);assert p.returncode==0;return log
    try:
        exe=out/'journal-replay';run(['wsl','--exec','g++','-std=c++20','-O2','-I'+linux(ROOT/'port/matrixgame/native-engine'),*[linux(ROOT/p) for p in files],'-o',linux(exe)],'build')
        measurements=[]
        # Sequential ABBA: no concurrent benchmark disk traffic.
        for i,mode in enumerate(('baseline','sampled','sampled','baseline')):
            log=run(['wsl','--exec',linux(exe),linux(tsv),linux(out/('filesystem-'+str(i))),mode],mode+'-'+str(i))
            fields=dict(re.findall(r'(input|written|pressure|elapsed_us)=(\d+)',log));fields={k:int(v) for k,v in fields.items()};assert fields['input']==7747 and fields['pressure']==591
            assert fields['written']==(7747 if mode=='baseline' else 671);measurements.append({'mode':mode,**fields})
        result['measurements']=measurements;result['median_elapsed_us']={mode:statistics.median(m['elapsed_us'] for m in measurements if m['mode']==mode) for mode in ('baseline','sampled')}
        result['source_capture_sha256']=sha(capture);result['status']='PASS HOST JOURNAL REPLAY; 7747 TO 671 RECORDS; SWITCH PERFORMANCE NOT MEASURED'
    finally:
        result['source_sha256']={p:sha(ROOT/p) for p in files+['port/matrixgame/native-engine/native_diagnostic_journal.hpp','port/matrixgame/native-engine/native_profile.hpp','tools/matrixgame_perf_benchmark.py']};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
