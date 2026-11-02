"""Verify native Windows hosted Init prefix evidence without running it again."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e578-hosted-windows-native'
def latest(folder,pattern,prefix):
 for path in sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True):
  result=read(path)
  if result['status'].startswith(prefix):return path,result
 raise AssertionError('missing completed result: '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 build_path,build=latest(BASE,'build-result*.json','PASS; FRESH')
 prefix_path,prefix=latest(BASE/'prefix','result*.json','PASS; WINDOWS REAL HOSTED')
 for result in (build,prefix):assert all(c['exit']==0 for c in result['commands'])
 check_hashes(build['archives_sha256']);check_hashes(build['image_inputs_sha256'])
 assert sha(ROOT/'tools/matrixgame_hosted_windows_native_build.py')==build['script_sha256']
 assert sha(BASE/'native/compile_commands.json')==build['compile_commands_sha256']
 generated=read(BASE/'native/source/native-engine-source.json');assert generated==build['generated']
 check_hashes(generated['files_sha256'],BASE/'native/source')
 all_commands=read(BASE/'native/compile_commands.json');objects={};sources={}
 # CMake also exports unrequested reference executable targets. Verify the
 # exact five archive targets that were actually built, never count those
 # unbuilt command plans as compiled objects.
 targets=('matrixgame-native-cache','matrixgame-native-vector-object','matrixgame-native-navigation','matrixgame-native-core','matrixgame-material-combiner')
 commands=[entry for entry in all_commands if any('/'+name+'.dir/' in '/'+entry['output'].replace('\\','/') for name in targets)]
 for entry in commands:
  assert '/DNDEBUG' not in entry['command'] and '-DNDEBUG' not in entry['command']
  assert '/UNDEBUG' in entry['command'] and '-MT' in entry['command']
  source=Path(entry['file']);obj=Path(entry['directory'])/entry['output']
  sources[str(source.relative_to(ROOT))]=sha(source);objects[str(obj.relative_to(ROOT))]=sha(obj)
 assert len(objects)==len(commands)==159
 check_hashes(prefix['source_sha256']);check_hashes(prefix['link_inputs_sha256']);check_hashes(prefix['fixture_inputs_sha256']);check_hashes(prefix['reused_host_objects_sha256'])
 assert len(prefix['reused_host_objects_sha256'])==337
 assert sha(ROOT/'tools/matrixgame_hosted_windows_native_checks.py')==prefix['script_sha256']
 assert sha(BASE/'prefix/prefix.obj')==prefix['object_sha256']
 assert sha(BASE/'prefix/hosted-native-prefix.exe')==prefix['binary_sha256']
 assert 'original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS' in prefix['stdout']
 assert prefix['stdout'].count('original Init exception: HOSTED_NATIVE_CAPS_BOUNDARY')==2
 assert prefix['stdout'].count('failure before cleanup: statics=1 heap=1 data=1 cache=1')==2
 host=read(ROOT/'build/matrixgame/e2e577-rangers-windows/result.json');check_hashes(host['objects_sha256']);check_hashes(host['source_sha256']);check_hashes(host['headers_sha256'])
 game=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/host/result.json')['protected_game_sha256'];check_hashes(game)
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 nro=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(nro)==build['NRO_before']=='9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356'
 old=ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(old)=='d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750'
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 pins={};git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; WINDOWS X64 REAL HOSTED NATIVE INIT PREFIX AND TWO ORIGINAL ROLLBACK CYCLES',
  'build_result':str(build_path.relative_to(ROOT)),'prefix_result':str(prefix_path.relative_to(ROOT)),
  'build':build,'prefix':prefix,'native_objects_sha256':objects,'native_sources_sha256':sources,'all_CMake_planned_units':len(all_commands),'built_archive_units':len(commands),
  'protected_game_sha256':game,'protected_frozen_sha256':protected,'clean_upstream_pins':pins,'E575_NRO_preserved':sha(nro),
  'scope':'Fresh MSVC x64 native MatrixGame production archives + real original Rangers host; genuine CGame::Init allocates/loads CPU prefix and invokes original text/sound callbacks before deliberate Unsupported ReadCaps. Two original cleanup/reentry cycles, no device-operation success emulation.',
  'remaining':['complete native Init through map construction/loading/interface and first frames','actual combined NRO execution and next boundary','Switch graphics/audio/HID/focus/F9/recovery'],
  'Windows_HAL':'NOT RUN','Switch':'NOT RUN','full_Init':'NOT RUN','new_NRO':False,'commit':False,'push':False,'audit_script_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print('native objects',len(objects),'reused host objects',len(prefix['reused_host_objects_sha256']));print('EXE',prefix['binary_sha256'])
if __name__=='__main__':main()
