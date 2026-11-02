"""Standard government SDK target and retained original UI link closure; reuse passed engine objects."""
import json,os,shutil,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e594-government-session/sdk'
def main():
 BASE.mkdir(parents=True,exist_ok=True);old=ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk';prior=read(old/'result.json');assert prior['status'].startswith('PASS');check_hashes(prior['link_inputs_sha256'])
 existing=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns)
 if existing:
  previous=read(existing[-1]);history=BASE/'history';history.mkdir(exist_ok=True);i=1
  while (history/str(i)).exists():i+=1
  snapshot=history/str(i);snapshot.mkdir()
  for path in [existing[-1],BASE/'native-diagnostic-build.json',*[ROOT/name for name in previous.get('artifacts',{})]]:
   if path.is_file():shutil.copyfile(path,snapshot/path.name);assert sha(path)==sha(snapshot/path.name)
 for name,info in prior['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 command=prior['commands'][0]['command'][:];command[command.index('-B')+1]=str(BASE)
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'runtime':'NOT RUN','Switch':'NOT RUN','new_NRO':False}
 def run(cmd,label,cwd=ROOT):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-6500:] if p.returncode else '',flush=True);assert not p.returncode,label;return log
 try:
  run(command,'configure');run([command[0],'--build',BASE,'--target','matrixgame-rangers-government','-j','4'],'build-government-target')
  archives=[p for p in BASE.rglob('libmatrixgame-rangers-government.a') if 'history' not in p.relative_to(BASE).parts];assert len(archives)==1;archive=archives[0]
  nm='C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe';defined=run([nm,'-g','--defined-only',archive],'government-API-symbols')
  roots=sorted({line.split()[-1] for line in defined.splitlines() if len(line.split())>=3 and line.split()[-2]=='T' and 'RangersGovernmentSession' in line.split()[-1]});assert len(roots)>=5
  args=next(c['command'] for c in prior['commands'] if c['label']=='link-real-API-closure')[:]
  elf=BASE/'matrixgame-government-link.elf';args[args.index('-o')+1]=str(elf);args=[('-Wl,-Map='+str(BASE/'government-link.map')) if v.startswith('-Wl,-Map=') else v for v in args]
  index=args.index('-Wl,--start-group')+1;args[index:index]=['-Wl,--whole-archive',str(archive),'-Wl,--no-whole-archive'];args += ['-Wl,--undefined='+name for name in roots]
  run(args,'link-retained-government-API',old)
  symbols=run([nm,'-g',elf],'elf-symbols');undefined=[line for line in symbols.splitlines() if ' U ' in line];assert not undefined and all(name in symbols for name in roots)
  header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header');assert 'AArch64' in header
  metadata=read(BASE/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'source');assert metadata['generated_original_sha256']==read(old/'native-diagnostic-build.json')['generated_original_sha256']
  check_hashes(prior['link_inputs_sha256'])
  for name,info in prior['artifacts'].items():assert sha(ROOT/name)==info['sha256']
  result.update(status='PASS; STANDARD GOVERNMENT SDK LIBRARY/RETAINED API ELF; ZERO STRONG UNDEFINED; NOT RUN',retained_API_symbols=roots,strong_undefined=undefined,elf_header=header,metadata_sha256=sha(BASE/'native-diagnostic-build.json'),artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (archive,elf,BASE/'government-link.map')},new_link_inputs_sha256={str(p.relative_to(ROOT)):sha(p) for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.a','.o','.obj') and 'history' not in p.relative_to(BASE).parts},reused_E592_link_inputs_sha256=prior['link_inputs_sha256'],preserved_E592_artifacts=prior['artifacts'])
 finally:
  path=BASE/'result.json';i=1
  while path.exists():path=BASE/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
