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
 inputs=[local(p) for p in manifest['objects'] if Path(p).name not in ('snapshot_probe.o','renderer_platform.o','GR_Main.o','win32_compat_messages.o','win32_compat_gdi.o','win32_compat_window.o')]
 for p in inputs:assert sha(p)==saved[str(p)],str(p)
 compile_command=next(c['command'] for c in prior['commands'] if c['label'].startswith('compile-'));compiler=compile_command[2];flags=compile_command[3:compile_command.index('-c')]
 flags+=['-I'+str(ROOT/'port/matrixgame/native'),'-I'+str(ROOT/'port/switch/platform'),'-I'+str(ROOT/'port/matrixgame/native-engine')]
 parent=ROOT/'build/matrixgame/e2e610-native-ui-pump'/('host-owner' if host else 'arm64-owner');parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();probe=out/'native_ui_pump_probe.cpp';text=(ROOT/'tests/probe_matrixgame_rangers_native_input.cpp').read_text(encoding='utf-8');anchor='m::native_input::EngineSink sink;';assert text.count(anchor)==1
 adapter='''struct ProbeSink {void Key(bool down,std::uint8_t key){auto* target=m::native_input::CurrentUiTarget();assert(target);target->Key(down,key);} void Move(int x,int y){auto* target=m::native_input::CurrentUiTarget();assert(target);target->Move(x,y);}} sink;'''
 probe.write_text(text.replace(anchor,adapter),encoding='utf-8')
 sdk=ROOT/'build/matrixgame/e2e610-native-ui-pump/sdk/3';production=sdk/'GR_Main.cpp';production_result=read(sdk/'result.json');assert production_result['status'].startswith('PASS') and sha(production)==production_result['sources_sha256'][str(production.relative_to(ROOT)).replace('/','\\')]
 main_source=out/'GR_Main.cpp';main_source.write_text(production.read_text(encoding='utf-8'),encoding='utf-8')
 sources=[probe,main_source,ROOT/'port/matrixgame/native/rangers_native_input.cpp',*[ROOT/'port/switch/platform'/name for name in ('win32_compat_messages.cpp','win32_compat_window.cpp','win32_compat_gdi.cpp')],ROOT/'port/matrixgame/native/rangers_ui_context.cpp',ROOT/'port/switch/platform/renderer_platform.cpp'];result={'status':'INCOMPLETE','commands':[],'profile':args.profile,'scope':'ACTUAL ORIGINAL GI/UI OWNER; SCRIPTED INPUT BOUNDARY; NO GPU OR HID','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_ui_context.hpp',ROOT/'port/matrixgame/native/rangers_native_input.hpp',ROOT/'port/switch/platform/rangers_ui_input.hpp',ROOT/'port/switch/platform/ui_control_cancel.hpp',ROOT/'port/matrixgame/native-engine/rangers_input_queue.hpp',ROOT/'port/matrixgame/native-engine/native_ui_target.hpp',ROOT/'port/matrixgame/native-engine/rangers_canvas_pixels.hpp',ROOT/'tests/probe_matrixgame_rangers_native_input.cpp',ROOT/'tools/matrixgame_native_ui_input_overrides.py',Path(__file__).resolve()]},'reused_objects_sha256':{str(p.relative_to(ROOT)):saved[str(p)] for p in inputs},'flags':flags}
 def run(command,label,env=None):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,timeout=600);log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-8500:] if p.returncode else '\n'.join(l for l in log.splitlines() if l.startswith('NATIVE_UI_PUMP')),flush=True)
  if label=='runtime':result['stdout']=log
  assert p.returncode==0,label
 try:
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.o');run([compiler,*flags,'-c',source,'-o',obj],'compile-'+source.stem);objects.append(obj)
  binary=out/'ui-context';manifest.update(objects=list(map(str,[*inputs,*objects])),output=str(binary),report=str(out/'link-report.json'));link_input=out/'link-input.json';link_input.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');run(['python3',ROOT/'tools/matrixgame_government_cpu_link.py',link_input],'link')
  runtime=out/'runtime';runtime.mkdir();env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');command=[binary,ROOT/'windows/Space Rangers HD A War Apart',runtime]
  if not host:command=['qemu-aarch64','-L','/usr/aarch64-linux-gnu',*command]
  run(command,'runtime',env);assert 'actual_original_GR_WinMessage_GI=PASS' in result['stdout'] and 'original_host_UI_script_cleanup=PASS' in result['stdout']
  for name,digest in result['sources_sha256'].items():assert sha(ROOT/name)==digest
  result.update(status='PASS '+('HOST ASAN/UBSAN/LSAN' if host else 'ARM64/QEMU')+' ACTUAL ORIGINAL GI/NATIVE UI PUMP/IMPORTS/OWNER CLEANUP; SCRIPTED INPUT BOUNDARY',objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary_sha256=sha(binary),new_NRO=False,Switch='NOT RUN')
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
