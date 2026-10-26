"""Actual original map/HAL capture and production native state replay. No NRO."""
import argparse,hashlib,json,os,re,subprocess,sys,time,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e536-actual-draw'
sys.stdout.reconfigure(encoding='utf-8')
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 global BASE
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['windows','host','arm64','sdk']);ap.add_argument('--output',type=Path,default=BASE);ap.add_argument('--selection',action='store_true');ap.add_argument('--reuse-compiled',action='store_true');ap.add_argument('--resource-root',type=Path);args=ap.parse_args();kind=args.kind;BASE=args.output.resolve()
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 paths=['tests/'+n for n in ('matrixgame_actual_draw_state.hpp','matrixgame_actual_draw_audit.hpp','matrixgame_actual_draw_windows.cpp','test_matrixgame_actual_draw_replay.cpp')]
 paths+=['port/matrixgame/native-engine/'+n for n in ('platform_win32.hpp','map_device_backend.cpp','shadow_texture_backend.cpp','debug_font_backend.cpp','native_device_state.cpp','native_device_state.hpp')]
 paths+=['port/matrixgame/deko/'+n for n in ('fixed_vertex.cpp','fixed_vertex.hpp','material_combiner.cpp','material_combiner.hpp','stencil_state.hpp','pixel_fog.hpp')]
 def run(cmd,label,cwd=ROOT):
  if isinstance(cmd,str):
   script=out/(label+'.cmd');script.write_text(cmd+'\n');cmd=['cmd.exe','/d','/c',script]
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=cwd,env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64',MATRIXGAME_UNION_OUTPUT=str(out),MATRIXGAME_DRAW_AUDIT_OUTPUT=str(out)),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=360)
  stdout=p.stdout+p.stderr;(out/(label+'.log')).write_text(stdout,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start})
  print(label,p.returncode,stdout[-6000:] if p.returncode or label=='runtime' else 'saved',flush=True)
  if label=='runtime':j['stdout']=stdout
  if p.returncode:j['status']='FAIL';raise RuntimeError(label+' failed')
  return stdout
 try:
  if kind=='windows':
   prior=ROOT/'build/matrixgame/e2e533-form/windows';template=(prior/'build-platform_win32.cmd').read_text().split(' /c ')[0]+' /I"'+str(ROOT/'tests')+'" /I"'+str(ROOT/'port/switch/platform')+'" /I"'+str(ROOT/'build/e2e515/deps/zlib-1.3.2')+'" /utf-8'
   j['compile_template']=template
   body=(ROOT/'tests/matrixgame_windows_form_loop.cpp').read_text();marker='    NativeInitRuntime runtime;OriginalGameOptions options;';assert body.count(marker)==1
   if args.selection:
    # The declaration must follow includes, since original engine SDK aliases
    # are intentionally isolated from the real Windows SDK translation units.
    declaration='void RunOriginalSelectionScenario(srhd_awa::matrixgame::platform3g::Device&,CFormMatrixGame&);\n'
    body=body.replace('namespace {\n',declaration+'namespace {\n',1)
    hook='            // Explicit optional-path guard:';assert body.count(hook)==1;body=body.replace(hook,'            RunOriginalSelectionScenario(device,form.Form());\n'+hook)
   body=body.replace(marker,'    matrixgame_draw_audit::Session draw_audit(device);\n'+marker)
   marker='        return 0;';assert body.count(marker)==1;body=body.replace(marker,'        draw_audit.Finish();\n'+marker)
   resource_define=''
   if args.resource_root:resource_define='#define MATRIXGAME_FORM_TEST_RESOURCE_ROOT L"'+args.resource_root.resolve().as_posix()+'"\n'
   harness=out/'driver.cpp';harness.write_text('#include "matrixgame_actual_draw_audit.hpp"\n#define MATRIXGAME_FORM_TEST_OUTPUT L"'+(out/('private-output-'+uuid.uuid4().hex)).as_posix()+'"\n'+resource_define+body,encoding='utf-8')
   main_body=(ROOT/'tests/matrixgame_windows_frame_main.cpp').read_text();marker='    return result;\n}';assert main_body.count(marker)==1
   main_body=main_body.replace(marker,'    return matrixgame_draw_audit::FinalResult(result);\n}')
   main_source=out/'matrixgame_windows_frame_main.cpp';main_source.write_text('#include "matrixgame_actual_draw_audit.hpp"\n'+main_body,encoding='utf-8')
   sources=[ROOT/'port/matrixgame/native-engine'/n for n in ('debug_font_backend.cpp','map_device_backend.cpp','offscreen_backend.cpp','original_game.cpp','init_runtime.cpp','platform_win32.cpp','shadow_texture_backend.cpp','text_backend.cpp','unmanaged_texture_backend.cpp','transition_backend.cpp','native_device_state.cpp')]
   sources += [ROOT/'port/matrixgame/native-core'/n for n in ('file_native.cpp','resource_paths.cpp')]
   # FILE owners and users must use the same CRT as the refreshed CFile TU.
   sources += [ROOT/'port/switch/platform/package.cpp']
   sources += [ROOT/'port/matrixgame/deko'/n for n in ('fixed_vertex.cpp','material_combiner.cpp')]+[ROOT/'tests'/n for n in ('matrixgame_windows_stored_slots.cpp','matrixgame_actual_draw_windows.cpp')]+[main_source,harness]
   sources += [ROOT/'port/matrixgame/native-engine/native_input.cpp',ROOT/'port/matrixgame/native-engine/native_input_engine.cpp']
   if args.selection:sources += [ROOT/'tests/matrixgame_windows_selection.cpp']
   paths += [str(p.relative_to(ROOT)) for p in sources]+['tests/matrixgame_windows_form_loop.cpp','tests/matrixgame_windows_frame_main.cpp']
   link=(prior/'link.cmd').read_text().strip();oldexe=str(prior/'form-loop.exe');assert oldexe in link
   link=link.replace(oldexe,str(out/'actual-draw.exe')).replace(str(prior/'form-loop.map'),str(out/'actual-draw.map'))
   previous=read(out/'result.json') if args.reuse_compiled and (out/'result.json').is_file() else {}
   headers=[p for folder in (ROOT/'port/matrixgame',ROOT/'tests',ROOT/'build/matrixgame/e2e528-minimap/source') for p in folder.rglob('*') if p.suffix in ('.h','.hpp') and p.is_file()]
   latest_header=max(p.stat().st_mtime_ns for p in headers)
   for source in sources:
    obj=out/(source.stem+'.obj');source_name=str(source.relative_to(ROOT))
    compiled_before=any(c['label']=='build-'+source.stem and c['exit']==0 for c in previous.get('commands',[]))
    if not compiled_before and obj.is_file():compiled_before=any(r['source']==source_name and r['object_sha256']==sha(obj) for r in previous.get('reused_compilation',[]))
    reusable=previous.get('compile_template')==template and obj.is_file() and previous.get('source_sha256',{}).get(source_name)==sha(source) and obj.stat().st_mtime_ns>=max(source.stat().st_mtime_ns,latest_header) and compiled_before
    if reusable:j.setdefault('reused_compilation',[]).append({'source':source_name,'object':str(obj.relative_to(ROOT)),'object_sha256':sha(obj),'all_local_headers_older_than_object':True})
    else:run(template+' /c "'+str(source)+'" /Fo"'+str(obj)+'"','build-'+source.stem)
    aliases={source.stem.lower()}
    if source.stem=='map_device_backend':aliases.add('map-device')
    existing=[s for s in re.findall(r'"([^"]+\.obj)"',link,re.I) if Path(s).stem.lower() in aliases]
    assert len(existing)<=1,(source,existing)
    if existing:link=link.replace('"'+existing[0]+'"','"'+str(obj)+'"')
    else:link+=' "'+str(obj)+'"'
   run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows');j['binary_sha256']=sha(out/'actual-draw.exe')
   cwd=read(ROOT/'build/matrixgame/e2e522/windows-frame/result.json')['cwd']
   if args.resource_root:
    cwd=out/'foreign-cwd';cwd.mkdir(exist_ok=True);j['resource_root']=str(args.resource_root.resolve());j['foreign_cwd']=str(cwd)
   run([out/'actual-draw.exe',out],'runtime',cwd)
   assert j['stdout'].count('ACTUAL_MAP_NATIVE_STATE')==2 and j['stdout'].count('REAL_FORM_LOOP draws=8 elapsed=125')==2
  elif kind in ('host','arm64'):
   paths+=['port/matrixgame/native-core/buffer_resources.cpp']
   sources=[ROOT/p for p in ('tests/test_matrixgame_actual_draw_replay.cpp','port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp')]
   original=ROOT/'build/matrixgame/e2e535-standalone-input/sdk/source';includes=[ROOT/p for p in ('tests','port/matrixgame/native-engine','port/matrixgame/native-core','port/matrixgame/deko')]+[original/'portable-core',original/'MatrixLib/Base']
   flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-pie','-no-pie','-pthread']
   if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
   exe=out/'replay';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*map(linux,sources),'-o',linux(exe)],'build');j['binary_sha256']=sha(exe)
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   captures=sorted((BASE/'windows').glob('draw-states-*.bin'));assert captures
   run(cmd+[linux(exe),*map(linux,captures)],'runtime');j['capture_sha256']={str(p.relative_to(ROOT)):sha(p) for p in captures}
  else:
   paths+=['port/matrixgame/native-core/resource_paths.hpp','port/matrixgame/native-core/resource_paths.cpp','port/matrixgame/native-core/file_native.cpp','port/matrixgame/native-core/CMakeLists.txt','port/matrixgame/native-engine/original_game.hpp','port/matrixgame/native-engine/original_game.cpp','port/matrixgame/native-engine/init_runtime.hpp','port/matrixgame/native-engine/init_runtime.cpp','tests/probe_matrixgame_native_entry.cpp']
   paths+=['port/matrixgame/native-engine/'+n for n in ('native_input.hpp','native_input.cpp','native_input_engine.hpp','native_input_engine.cpp','native_text.hpp','native_text.cpp','native_text_backend.cpp','native_text_asset.cpp','text_backend.cpp','platform_switch.cpp','native_draw_batch.hpp','native_frame_scope.hpp','native_command_memory.hpp')]+['port/matrixgame/deko/legacy_raster_viewport.hpp']
   cmd=read(ROOT/'build/matrixgame/e2e535-standalone-input/sdk/result.json')['commands'][0]['command'][:];cmd[cmd.index('-B')+1]=str(out)
   run(cmd,'configure');run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build')
   symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',out/'matrixgame-native-entry.elf'],'symbols');j['strong_undefined']=[s for s in symbols.splitlines() if ' U ' in s];assert not j['strong_undefined'];j['elf_sha256']=sha(out/'matrixgame-native-entry.elf')
   ninja=next(v.split('=',1)[1] for v in cmd if v.startswith('-DCMAKE_MAKE_PROGRAM='));graph=run([ninja,'-C',out,'-t','graph','matrixgame-native-entry.elf'],'link-graph');inputs=set()
   for label in re.findall(r'\[label="((?:\\.|[^"\\])*)"',graph):
    p=(out/Path(json.loads('"'+label+'"'))).resolve()
    if p.is_relative_to(ROOT) and p.suffix in ('.o','.obj','.a') and p.is_file():inputs.add(p)
   j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sorted(inputs)}
   generated=read(out/'source/native-engine-source.json')['files_sha256'];assert generated==read(ROOT/'build/matrixgame/e2e535-standalone-input/sdk/source/native-engine-source.json')['files_sha256'];j['original_generated_files_unchanged']=len(generated)
  j['status']='PASS' if kind!='sdk' else 'COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN'
 finally:
  j['source_sha256']={p:sha(ROOT/p) for p in set(paths)}
  j['capture_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sorted((BASE/'windows').glob('draw-states-*.bin'))}
  (out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
