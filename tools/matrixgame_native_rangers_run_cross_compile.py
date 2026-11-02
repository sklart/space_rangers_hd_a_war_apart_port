"""Compile the complete native Run adapter with host sanitizers and ARM64."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run'
ENGINE=ROOT/'build/matrixgame/e2e590-completed-battle/fixed/generated'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 source=ROOT/'port/matrixgame/native-engine/native_rangers_run.cpp'
 for kind in ('host','arm64'):
  out=BASE/('compile-'+kind);out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'runtime':'NOT RUN','Switch':'NOT RUN'}
  prior=read(ROOT/f'build/matrixgame/e2e568-native-services/{kind}/result.json');template=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime');flags=[];i=3
  while i<len(template):
   if template[i] in ('-c','-o'):i+=2;continue
   flags.append(template[i].replace(linux(ROOT/'build/matrixgame/e2e567-screenshot/sdk/source'),linux(ENGINE)));i+=1
  flags+=['-I'+linux(ROOT/p) for p in ('upstream/cpp/src','upstream/cpp/runtime','port/matrixgame/native')]
  try:
   obj=out/'native_rangers_run.o';command=[*template[:3],*flags,'-c',linux(source),'-o',linux(obj)]
   p=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=out/'compile.log';index=1
   while path.exists():path=out/f'compile-{index}.log';index+=1
   path.write_text(log,encoding='utf-8');result['commands'].append({'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(kind,p.returncode,log[-4000:] if p.returncode else '',flush=True);assert not p.returncode
   result.update(status='PASS; '+kind.upper()+' COMPLETE NATIVE RANGERS RUN ADAPTER COMPILE; NO EXECUTION',source_sha256={str(p.relative_to(ROOT)):sha(p) for p in (source,source.with_suffix('.hpp'),Path(__file__).resolve())},objects_sha256={str(obj.relative_to(ROOT)):sha(obj)})
  finally:
   path=out/'result.json';index=1
   while path.exists():path=out/f'result-{index}.json';index+=1
   path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
