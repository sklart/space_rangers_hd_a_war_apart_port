"""Compile changed AI translation unit and real-world oracle; no runtime claim."""
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
BASE=ROOT/'build/matrixgame/e2e582-ai-ignore'
ENGINE=BASE/'native-build/native/source'
SOURCES=None
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 for kind in ('host','arm64'):
  out=BASE/('compile-'+kind);out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'runtime':'NOT RUN','Switch':'NOT RUN'}
  sources=SOURCES or [ENGINE/'MatrixGame/src/Logic/MatrixEnvironment.cpp',ROOT/'tests/matrixgame_ai_ignore_queue.cpp']
  prior=read(ROOT/f'build/matrixgame/e2e568-native-services/{kind}/result.json');template=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime');flags=[];i=3
  while i<len(template):
   if template[i] in ('-c','-o'):i+=2;continue
   flags.append(template[i].replace(linux(ROOT/'build/matrixgame/e2e567-screenshot/sdk/source'),linux(ENGINE)));i+=1
  try:
   objects=[]
   for source in sources:
    obj=out/(source.stem+'.o');command=[*template[:3],*flags,'-c',linux(source),'-o',linux(obj)]
    p=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=out/(source.stem+'.log');index=1
    while path.exists():path=out/f'{source.stem}-{index}.log';index+=1
    path.write_text(log,encoding='utf-8');result['commands'].append({'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(kind,source.name,p.returncode,log[-4000:] if p.returncode else '',flush=True);assert not p.returncode;objects.append(obj)
   result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources};result['objects_sha256']={str(p.relative_to(ROOT)):sha(p) for p in objects}
   result['status']='PASS; '+kind.upper()+' AI IGNORE DELTA COMPILE ONLY; NO EXECUTION'
  finally:
   result['script_sha256']=sha(Path(__file__).resolve());path=out/'result.json';index=1
   while path.exists():path=out/f'result-{index}.json';index+=1
   path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
