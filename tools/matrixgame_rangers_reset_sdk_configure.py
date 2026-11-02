"""Check the Windows-only Reset boundary is excluded from the libnx graph; no rebuild."""
import json,os,subprocess
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e593-rangers-caller-reset/sdk-configure'
def main():
 BASE.mkdir(parents=True,exist_ok=True);prior=read(ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk/result.json');assert prior['status'].startswith('PASS')
 for name,info in prior['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 command=prior['commands'][0]['command'][:];command[command.index('-B')+1]=str(BASE)
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64');result={'status':'INCOMPLETE','commands':[],'compile':'NOT RUN; WINDOWS-ONLY CHANGE','Switch':'NOT RUN'}
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,env=env,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,(p.stdout+p.stderr)[-4000:] if p.returncode else '',flush=True);assert not p.returncode;return p.stdout+p.stderr
 try:
  run(command,'configure');cache=(BASE/'CMakeCache.txt').read_text(encoding='utf-8');ninja=next(l.split('=',1)[1] for l in cache.splitlines() if l.startswith('CMAKE_MAKE_PROGRAM:FILEPATH='))
  graph=run([ninja,'-C',BASE,'-t','commands','matrixgame-native-rangers-interface'],'inspect-interface-graph')
  assert 'rangers_legacy_presentation_win32.cpp' not in graph and 'native_rangers_interface.cpp' in graph and 'aarch64-none-elf' in graph
  metadata=read(BASE/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'source')
  previous=read(ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk/native-diagnostic-build.json');assert metadata['generated_original_sha256']==previous['generated_original_sha256']
  result.update(status='PASS; DEVKITA64 CONFIGURE/GRAPH ONLY; WINDOWS RESET SOURCE EXCLUDED; NO REBUILD/NEW NRO',metadata_sha256=sha(BASE/'native-diagnostic-build.json'),generated_originals_unchanged=True,preserved_E592_artifacts=prior['artifacts'])
 finally:
  path=BASE/'result.json';i=1
  while path.exists():path=BASE/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
