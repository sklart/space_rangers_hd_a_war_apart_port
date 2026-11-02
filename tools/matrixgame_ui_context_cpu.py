"""Real host sanitizer/ARM64 UI context with late renderer allocation failure."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):
 with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def local(name):
 name=str(name).replace('\\','/').replace('D:/','/mnt/d/');return Path(name) if name.startswith('/') else ROOT/name
def main():
 parser=argparse.ArgumentParser();parser.add_argument('profile',choices=('host','arm64'));args=parser.parse_args();host=args.profile=='host'
 baseline=ROOT/('build/matrixgame/e2e601-revolution-seed/1' if host else 'build/matrixgame/e2e600-campaign-navigation/snapshot-arm64/2');prior=read(baseline/'result.json');assert prior['status'].startswith('PASS');manifest=read(baseline/'link-input.json')
 saved={str(local(k)):v for k,v in {**prior.get('reused_objects_sha256',{}),**prior.get('objects_sha256',{})}.items()}
 if host:saved[str(baseline/'aPlanet.o')]=prior['object_sha256']
 inputs=[local(p) for p in manifest['objects'] if Path(p).name not in ('snapshot_probe.o','renderer_platform.o')]
 for p in inputs:assert sha(p)==saved[str(p)],str(p)
 compile_command=next(c['command'] for c in prior['commands'] if c['label'].startswith('compile-'));compiler=compile_command[2];flags=compile_command[3:compile_command.index('-c')]
 flags+=['-I'+str(ROOT/'port/matrixgame/native'),'-I'+str(ROOT/'port/switch/platform'),'-DMATRIXGAME_UI_CONTEXT_REALLOC_WRAP=1']
 parent=ROOT/'build/matrixgame/e2e607-ui-context'/args.profile;parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();sources=[ROOT/'tests/probe_matrixgame_ui_context.cpp',ROOT/'port/matrixgame/native/rangers_ui_context.cpp',ROOT/'port/switch/platform/renderer_platform.cpp'];result={'status':'INCOMPLETE','commands':[],'profile':args.profile,'scope':'CPU ORIGINAL UI CONTEXT AND LATE REALLOC FAILURE; NO GPU','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_ui_context.hpp',Path(__file__).resolve()]},'reused_objects_sha256':{str(p.relative_to(ROOT)):saved[str(p)] for p in inputs},'flags':flags}
 def run(command,label,env=None):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,timeout=600);log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-8500:] if p.returncode else '\n'.join(l for l in log.splitlines() if l.startswith('UI_CONTEXT')),flush=True)
  if label=='runtime':result['stdout']=log
  assert p.returncode==0,label
 try:
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.o');run([compiler,*flags,'-c',source,'-o',obj],'compile-'+source.stem);objects.append(obj)
  binary=out/'ui-context';manifest.update(objects=list(map(str,[*inputs,*objects])),output=str(binary),report=str(out/'link-report.json'));manifest['link_flags']+=['-Wl,--wrap=realloc','-Wl,--wrap=malloc'];link_input=out/'link-input.json';link_input.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');run(['python3',ROOT/'tools/matrixgame_government_cpu_link.py',link_input],'link')
  runtime=out/'runtime';runtime.mkdir();env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');command=[binary,ROOT/'windows/Space Rangers HD A War Apart',runtime]
  if not host:command=['qemu-aarch64','-L','/usr/aarch64-linux-gnu',*command]
  run(command,'runtime',env);assert result['stdout'].count('original_CFG_TCC_canvas_palette_cursors=PASS original_script_global_UI=PASS')==2 and 'actual_late_realloc_failure=PASS' in result['stdout'] and 'cleanup=PASS' in result['stdout']
  for name,digest in result['sources_sha256'].items():assert sha(ROOT/name)==digest
  result.update(status='PASS '+('HOST ASAN/UBSAN/LSAN' if host else 'ARM64/QEMU')+' ORIGINAL UI CONTEXT TWO CYCLES/LATE ALLOCATION FAILURE/CLEANUP',objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary_sha256=sha(binary),new_NRO=False,Switch='NOT RUN')
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
