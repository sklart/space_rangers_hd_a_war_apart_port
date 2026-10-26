"""Full original Windows Init/frame with fresh output/minimap/runtime objects."""
import hashlib,json,os,re,subprocess,sys,uuid
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e527-output';OUT=BASE/'windows-frame';SOURCE=BASE/'source'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 OUT.mkdir(parents=True,exist_ok=True);prior=json.loads((ROOT/'build/matrixgame/e2e525-surfaces/windows-frame/result.json').read_text());recipe=json.loads((ROOT/'build/matrixgame/e2e523-presentation/windows-frame/result.json').read_text());j={'status':'INCOMPLETE','commands':[]}
 def run(command,label,cwd=ROOT):
  script=OUT/(label+'.cmd');script.write_text(command+'\n');p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2600:] if p.returncode else '',flush=True);assert p.returncode==0
 try:
  output_root=OUT/('native-output-'+uuid.uuid4().hex);j['native_output_root']=str(output_root)
  observed=OUT/'MatrixMinimapOutputObserved.cpp';observed_text=(SOURCE/'MatrixGame/src/MatrixMinimap.cpp').read_text();
  for marker,label in [('lgr.debug("Rendering minimap background from scratch");','MAPCACHE_COLD_RENDER'),('lgr.debug("Stored minimap into cache file");','MAPCACHE_STORED'),('lgr.debug("Loaded minimap from cache file");','MAPCACHE_LOADED')]:
   assert observed_text.count(marker)==1;observed_text=observed_text.replace(marker,marker+' std::puts("'+label+'");')
  observed.write_text('#include <cstdio>\n'+observed_text)
  harness=OUT/'matrixgame_windows_frame_init_output.cpp';text=(ROOT/'tests/matrixgame_windows_frame_init.cpp').read_text();needle='    using namespace srhd_awa::matrixgame;';assert text.count(needle)==1;text=text.replace(needle,needle+'\n    output_paths::NativeScope private_outputs(L"'+output_root.as_posix()+'");');harness.write_text(text);j['harness_observation']='Only adds an explicit private native output binding around the original Windows Init/Draw/Deinit; no game algorithm replacement.'
  sources=[ROOT/'port/matrixgame/native-engine'/n for n in ('init_runtime.cpp','original_game.cpp','output_paths_backend.cpp','offscreen_scope.cpp')]+[SOURCE/'MatrixGame/src'/n for n in ('MatrixGamePathUtils.cpp',)]+[SOURCE/'MatrixLib/3G'/n for n in ('OriginalUnmanagedTexture.cpp','OriginalShadowTexture.cpp')]+[observed,harness]
  objects=[]
  template=recipe['commands'][0]['command'];template=template.replace(str(ROOT/'build/matrixgame/e2e521/source'),str(SOURCE))
  for source in sources:
   obj=OUT/(source.stem+'.obj');objects.append(obj);command=template[:template.index(' /c ')]+' /c "'+str(source)+'" /Fo"'+str(obj)+'"';run(command,'build-'+source.stem)
  link=next(c['command'] for c in prior['commands'] if c['label']=='link')
  replacements={ROOT/'build/matrixgame/e2e523-presentation/windows-frame/init_runtime.obj':objects[0],ROOT/'build/matrixgame/e2e521/windows/owner.obj':objects[1],ROOT/'build/matrixgame/e2e521/windows/engine-main.obj':objects[-1]}
  used=set()
  for old,new in replacements.items():assert str(old) in link;link=link.replace(str(old),str(new));used.add(new)
  # Fresh shadow/scope objects replace the E2E-5.25 explicit overrides.
  for obj in objects:
   old=ROOT/'build/matrixgame/e2e525-surfaces/windows-frame'/obj.name
   if str(old) in link:link=link.replace(str(old),str(obj));used.add(obj)
  link=link.replace(str(ROOT/'build/matrixgame/e2e525-surfaces/windows-frame/windows-game.exe'),str(OUT/'windows-game.exe')).replace(str(ROOT/'build/matrixgame/e2e525-surfaces/windows-frame/windows-game.map'),str(OUT/'windows-game.map'))
  link+=' '+' '.join('"'+str(p)+'"' for p in objects if p not in used);run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
  old=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text());cmd=[str(OUT/'windows-game.exe'),str(OUT)];p=subprocess.run(cmd,cwd=old['cwd'],env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=240);(OUT/'runtime.log').write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':'runtime','command':cmd,'exit':p.returncode});j['stdout']=p.stdout;print('runtime',p.returncode,(p.stdout+p.stderr)[-3500:],flush=True);assert p.returncode==0
  assert p.stdout.count('original CGame::Init returned: ready=1')==2 and p.stdout.count('actual SDK draws=1008')==2
  for marker in ('MAPCACHE_COLD_RENDER','MAPCACHE_STORED','MAPCACHE_LOADED'):assert p.stdout.count(marker)==1,(marker,p.stdout.count(marker))
  j['snapshots']={n:sha(OUT/n) for n in prior['snapshots']};assert j['snapshots']==prior['snapshots'];j['output_files_sha256']={str(p.relative_to(OUT)):sha(p) for p in output_root.rglob('*') if p.is_file()};assert j['output_files_sha256']
  j['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+[SOURCE/'MatrixGame/src/MatrixMinimap.cpp',ROOT/'port/matrixgame/native-engine/init_runtime.hpp',ROOT/'port/matrixgame/native-engine/original_game.hpp',ROOT/'port/matrixgame/native-engine/output_paths_backend.hpp',ROOT/'tools/matrixgame_minimap_overrides.py',ROOT/'tools/matrixgame_output_paths_overrides.py',Path(__file__).resolve()]};j['status']='PASS'
 finally:(OUT/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
