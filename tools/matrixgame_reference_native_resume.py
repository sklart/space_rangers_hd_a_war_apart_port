"""Resume the isolated MSVC build after correcting the JPEG header path."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment,CMAKE
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123';OUT=BASE/'native-windows'
def main():
 prior=json.loads((OUT/'result.json').read_text(encoding='utf8'));assert prior['status']=='INCOMPLETE'
 env=environment(OUT);result={'status':'INCOMPLETE','commands':[]}
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 try:
  run('configure-jpeg-retry',[CMAKE,'-S',ROOT/'port/matrixgame/native-engine','-B',OUT,'-DJPEG_INCLUDE_DIR='+str(BASE/'source-deps/libjpeg-turbo-3.1.0/src')])
  run('build-jpeg-retry',[CMAKE,'--build',OUT,'--target','matrixgame-native-cache','-j','4'])
  archives=list(OUT.rglob('*.lib'));assert len(archives)>=4
  result['archives']={str(p.relative_to(ROOT)):hashlib.file_digest(p.open('rb'),'sha256').hexdigest() for p in archives}
  result['status']='PASS; MSVC x64 MatrixGame archives only; runtime NOT RUN'
 finally:(OUT/'result-jpeg-retry.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
