"""Standard SDK library target and link closure with every real Run API method retained."""
import json,os,re,shlex,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run/sdk'
def main():
 BASE.mkdir(parents=True,exist_ok=True);prior=read(ROOT/'build/matrixgame/e2e590-completed-battle/sdk/variants-result.json')['interactive']
 command=prior['commands'][0]['command'][:];command[command.index('-B')+1]=str(BASE);command+=['-DMATRIXGAME_BUILD_RANGERS_RUN=ON']
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'runtime':'NOT RUN','Switch':'NOT RUN','new_NRO':False}
 def run(cmd,label,cwd=ROOT):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=BASE/(label+'.log');index=1
  while path.exists():path=BASE/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-7000:] if p.returncode else '',flush=True);assert not p.returncode,label;return log
 try:
  protected=read(ROOT/'reports/matrixgame-e2e590-completed-battle-evidence.json')['packages']
  for package in protected.values():assert sha(ROOT/package['path'])==package['sha256']
  run(command,'configure');run([command[0],'--build',BASE,'--target','matrixgame-native-rangers-run','matrixgame-native-interactive','-j','4'],'build-standard-targets')
  archive=BASE/'libmatrixgame-native-rangers-run.a';assert archive.is_file()
  nm=Path('C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe');defined=run([nm,'-g','--defined-only',archive],'API-defined-symbols')
  roots=sorted({line.split()[-1] for line in defined.splitlines() if len(line.split())>=3 and line.split()[-2]=='T' and 'NativeRangersRunBinding' in line.split()[-1]});assert len(roots)>=6
  cache=(BASE/'CMakeCache.txt').read_text(encoding='utf-8');ninja=next(line.split('=',1)[1] for line in cache.splitlines() if line.startswith('CMAKE_MAKE_PROGRAM:FILEPATH='))
  commands=run([ninja,'-C',BASE,'-t','commands','matrixgame-native-interactive.elf'],'generated-link-command')
  lines=[line for line in commands.splitlines() if 'aarch64-none-elf-g++.exe' in line and ' -o matrixgame-native-interactive.elf' in line];assert len(lines)==1,lines
  line=lines[0];match=re.search(r'(?i)([A-Z]:[\\/][^\s"]*aarch64-none-elf-g\+\+\.exe)',line);assert match,line
  raw=line[match.start():].split(' && ')[0];args=[v[1:-1] if v.startswith('"') and v.endswith('"') else v for v in shlex.split(raw,posix=False)]
  assert all(v not in ('&&',';','|') for v in args)
  elf=BASE/'matrixgame-native-rangers-run-link.elf';args[args.index('-o')+1]=str(elf)
  args=[('-Wl,-Map='+str(BASE/'rangers-run-link.map')) if v.startswith('-Wl,-Map=') else v for v in args]
  args+=['-Wl,--whole-archive',str(archive),'-Wl,--no-whole-archive',*['-Wl,--undefined='+name for name in roots]]
  run(args,'link-real-API-closure',BASE)
  symbols=run([nm,'-g',elf],'symbols');undefined=[line for line in symbols.splitlines() if ' U ' in line];assert not undefined
  assert all(name in symbols for name in roots)
  header=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header');assert 'AArch64' in header
  metadata=read(BASE/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'source');check_hashes(metadata['rangers_host']['archives_sha256'])
  for package in protected.values():assert sha(ROOT/package['path'])==package['sha256']
  result.update(status='PASS; STANDARD DEVKITA64 NATIVE RANGERS RUN LIBRARY AND REAL API LINK CLOSURE; ZERO STRONG UNDEFINED; NOT RUN',retained_API_symbols=roots,strong_undefined=undefined,elf_header=header,metadata_sha256=sha(BASE/'native-diagnostic-build.json'),artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (archive,elf,BASE/'rangers-run-link.map')},link_inputs_sha256={str(p.relative_to(ROOT)):sha(p) for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.a','.o','.obj')},preserved_E590_packages=protected)
 finally:
  path=BASE/'result.json';index=1
  while path.exists():path=BASE/f'result-{index}.json';index+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
