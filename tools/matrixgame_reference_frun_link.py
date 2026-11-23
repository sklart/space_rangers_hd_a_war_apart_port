"""Re-link the preserved E608 Gov scenario against freshly rebuilt MSVC inputs."""
import json,shutil,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment,compiler
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
OLD=ROOT/'build/matrixgame/e2e608-ui-campaign/windows-difficulty3-economy-campaign0-winter1_1e-heavy-squad-caller-lease/1'
def main():
 out=BASE/'gov-frun-link-3'
 if out.exists():raise RuntimeError('reference link output already exists')
 out.mkdir();env=environment(out);env['VSLANG']='1033';result={'status':'INCOMPLETE','commands':[]}
 host=json.loads((BASE/'rangers-windows-2/result.json').read_text(encoding='utf8'));assert host['status'].startswith('PASS')
 native=json.loads((BASE/'native-windows-run/result.json').read_text(encoding='utf8'));assert native['status'].startswith('PASS')
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 try:
  tree=BASE/'native-windows-run/source';core=BASE/'native-windows-run/core/source';engine=ROOT/'port/matrixgame/native-engine'
  extra=[tree/'MatrixLib/3G',tree/'MatrixLib/Base',tree/'MatrixLib/Bitmap',tree/'MatrixGame/src',tree/'MatrixGame',
         tree/'MatrixGame/src/Logic',tree/'MatrixGame/src/Effects',core,BASE/'deps/d3dx/build/native/include',
         ROOT/'port/matrixgame/third_party/directxmath/Inc',ROOT/'port/matrixgame/deko',OLD,ROOT/'tests']
  flags=['/I'+str(p) for p in extra]+host['flags']+['/DMATRIXGAME_CAMPAIGN_SQUAD=1','/DMATRIXGAME_CAMPAIGN_HEAVY_DESIGN=1',
          '/DMATRIXGAME_GOV_DIFFICULTY=3','/DMATRIXGAME_CAMPAIGN_DIFFICULTY=0']
  sources=[OLD/'campaign_frun.cpp',OLD/'campaign_sdk_main.cpp',engine/'rangers_caller_presentation.cpp',tree/'MatrixGame/src/OriginalWindowsFrame.cpp',ROOT/'tests/matrixgame_rangers_reference_window.cpp']
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.obj');run('compile-'+source.stem,[compiler(env),*flags,'/c',source,'/Fo'+str(obj)]);objects.append(obj)
  rc=shutil.which('rc.exe',path=env.get('PATH'));assert rc
  resource=out/'frame.rc';resource.write_text('1234 FONT "'+(ROOT/'upstream/matrixgame/MatrixGame/Rangers.ttf').as_posix()+'"\n',encoding='utf8')
  res=out/'frame.res';run('resource',[rc,'/nologo','/fo'+str(res),resource])
  objects += [ROOT/x['object'] for x in host['compiled']]
  libraries=[BASE/'native-windows-run/matrixgame-native-rangers-interface.lib',BASE/'native-windows-run/matrixgame-native-rangers-run.lib',
      BASE/'native-windows-run/matrixgame-native-cache.lib',BASE/'native-windows-run/matrixgame-native-vector-object.lib',
      BASE/'native-windows-run/matrixgame-material-combiner.lib',BASE/'native-windows-run/matrixgame-native-navigation.lib',
      BASE/'native-windows-run/core/matrixgame-native-core.lib',BASE/'okgf-windows/okgf.lib',
      BASE/'deps/sdl2/SDL2-2.30.11/lib/x64/SDL2.lib',BASE/'audio-libs/prefix/lib/vorbisfile.lib',
      BASE/'audio-libs/prefix/lib/vorbis.lib',BASE/'audio-libs/prefix/lib/ogg.lib',BASE/'image-libs/png/libpng16_static.lib',
      BASE/'image-libs/jpeg/jpeg-static.lib',BASE/'image-libs/zlib/zs.lib',BASE/'deps/d3dx/build/native/release/lib/x64/d3dx9.lib']
  assert all(p.is_file() for p in objects+libraries)
  rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*objects,res,*libraries]),encoding='utf8')
  run('link',[compiler(env),'/nologo','/MT','@'+str(rsp),'/Fe'+str(out/'gov-frun.exe'),'/link','/INCREMENTAL:NO',
      'd3d9.lib','dxguid.lib','oleaut32.lib','ole32.lib','user32.lib','gdi32.lib','version.lib','winmm.lib',
      'setupapi.lib','imm32.lib','advapi32.lib','shell32.lib'])
  result['status']='PASS; Windows Gov/FRun executable linked; runtime NOT RUN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
