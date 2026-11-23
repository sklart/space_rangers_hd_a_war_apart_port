"""Link and run real Rangers host services with fresh MSVC reference objects."""
import json,os,shutil,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment,compiler
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
def main():
 out=BASE/'rangers-host-probe'
 if out.exists():raise RuntimeError('host probe already exists')
 out.mkdir();env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy',VSLANG='1033')
 result={'status':'INCOMPLETE','commands':[]};previous=json.loads((BASE/'rangers-windows-2/result.json').read_text(encoding='utf8'));assert previous['status'].startswith('PASS')
 def run(label,cmd,cwd=ROOT):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
  return p.stdout+p.stderr
 try:
  obj=out/'probe.obj';run('compile',[compiler(env),*previous['flags'],'/c',ROOT/'tests/probe_matrixgame_rangers_host_session.cpp','/Fo'+str(obj)])
  native={'rangers_bridge.obj','engine_callbacks.obj','rangers_host_callbacks.obj','rangers_audio_session.obj',
          'rangers_music_session.obj','rangers_data_session.obj','rangers_host_session.obj'}
  objects=[ROOT/x['object'] for x in previous['compiled'] if '/objects/native/' not in x['object'].replace('\\','/') or Path(x['object']).name in native]
  libraries=[BASE/'okgf-windows/okgf.lib',BASE/'deps/sdl2/SDL2-2.30.11/lib/x64/SDL2.lib',
             BASE/'audio-libs/prefix/lib/vorbisfile.lib',BASE/'audio-libs/prefix/lib/vorbis.lib',BASE/'audio-libs/prefix/lib/ogg.lib',
             BASE/'image-libs/png/libpng16_static.lib',BASE/'image-libs/jpeg/jpeg-static.lib',BASE/'image-libs/zlib/zs.lib']
  assert all(x.is_file() for x in objects+libraries)
  rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*objects,obj,*libraries]),encoding='utf8')
  binary=out/'rangers-host.exe'
  run('link',[compiler(env),'/nologo','/MT','@'+str(rsp),'/Fe'+str(binary),'/link','/INCREMENTAL:NO',
      'oleaut32.lib','ole32.lib','user32.lib','gdi32.lib','version.lib','winmm.lib','setupapi.lib','imm32.lib','advapi32.lib','shell32.lib'])
  shutil.copyfile(BASE/'deps/sdl2/SDL2-2.30.11/lib/x64/SDL2.dll',out/'SDL2.dll')
  runtime=out/'runtime';runtime.mkdir()
  log=run('runtime',[binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],runtime)
  assert 'modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS' in log
  result['status']='PASS; original Rangers services MSVC x64, SDL dummy; no battle or Switch'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
