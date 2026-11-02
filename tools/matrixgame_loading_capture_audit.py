"""Audit E580 source, targeted execution, independent pixels and SDK package."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_screenshot_png_verify import decode,verify_fixture
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e580-loading-capture'
def latest(folder,pattern='result*.json'):
 for path in sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True):
  value=read(path)
  if value['status'].startswith('PASS'):return value
 raise AssertionError('no PASS in '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 profiles={}
 for kind in ('windows','host','arm64','windows-gpu'):
  record=latest(BASE/('capture-'+kind));assert all(c['exit']==0 for c in record['commands']);check_hashes(record['source_sha256'])
  if 'archives_sha256' in record:check_hashes({name.replace('/mnt/d/','D:/'):value for name,value in record['archives_sha256'].items()})
  binary=BASE/('capture-'+kind)/('capture.exe' if kind.startswith('windows') else 'capture');assert sha(binary)==record['binary_sha256']
  profiles[kind]=record
 png={}
 for kind in ('windows','host','arm64'):
  record=latest(BASE/'screenshot'/kind);assert all(c['exit']==0 for c in record['commands']);check_hashes(record['source_sha256']);check_hashes(record['output_sha256'])
  png[kind]={'execution':record,'independent_oracle':verify_fixture(ROOT/record['fixture'])}
 mapped=latest(BASE/'reference-map');assert all(c['exit']==0 for c in mapped['commands'])
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(mapped[key])
 assert mapped['stdout'].count('full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS')==2
 assert mapped['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and mapped['stdout'].count('readback_present=PASS')==8
 images={}
 for path in sorted((BASE/'reference-map/runtime/output').glob('map-frame-*.png')):
  width,height,rgb=decode(path.read_bytes(),allow_alpha=True);assert (width,height)==(800,600)
  colors=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert colors>=64
  images[str(path.relative_to(ROOT))]={'sha256':sha(path),'rgb_sha256':hashlib.sha256(rgb).hexdigest(),'distinct_rgb':colors}
 assert len(images)==8
 prefix=latest(BASE/'native-build/prefix');assert all(c['exit']==0 for c in prefix['commands'])
 check_hashes(prefix['source_sha256']);check_hashes(prefix['link_inputs_sha256'])
 assert prefix['stdout'].count('original Init exception: LOADING_CAPTURE_REFUSED')==2 and prefix['stdout'].count('original Init exception: HOSTED_NATIVE_CAPS_BOUNDARY')==2
 assert 'exclusive_scope_released=PASS real_host_preserved=PASS' in prefix['stdout']
 native=latest(BASE/'native-build','build-result*.json');check_hashes(native['archives_sha256']);check_hashes(native['image_inputs_sha256'])
 generated=read(BASE/'native-build/native/source/native-engine-source.json');assert generated==native['generated'];check_hashes(generated['files_sha256'],BASE/'native-build/native/source')
 objects={};targets=('matrixgame-native-cache','matrixgame-native-vector-object','matrixgame-native-navigation','matrixgame-native-core','matrixgame-material-combiner')
 for entry in read(BASE/'native-build/native/compile_commands.json'):
  if not any('/'+name+'.dir/' in '/'+entry['output'].replace('\\','/') for name in targets):continue
  assert '/DNDEBUG' not in entry['command'] and '/UNDEBUG' in entry['command']
  obj=Path(entry['directory'])/entry['output'];objects[str(obj.relative_to(ROOT))]=sha(obj)
 assert len(objects)==160
 sdk=latest(BASE/'sdk');assert all(c['exit']==0 for c in sdk['commands']) and not sdk['strong_undefined']
 check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 metadata=read(BASE/'sdk/native-diagnostic-build.json');check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 assert metadata['rangers_host']['enabled'] and sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256']
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 symbols=(BASE/'sdk/symbols.log').read_text();assert 'LoadingCaptureScope::LoadingCaptureScope(' in symbols and 'CaptureLoadingImage(' in symbols
 transition=(BASE/'sdk/source/MatrixGame/src/OriginalTransition.cpp').read_text();assert 'CaptureLoadingImage(bm, m_ScreenX, m_ScreenY)' in transition and 'CompatibleBitmap(hdc' not in transition
 nro=BASE/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';nacp=BASE/'sdk/diagnostic-nro/control.nacp'
 verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes())
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 game=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/host/result.json')['protected_game_sha256'];check_hashes(game)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 old_nro=ROOT/'build/matrixgame/e2e579-hosted-windows-map/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(old_nro)=='020b6d3debff52d5ae1efb757b2aee10b6e9886692d83a2d2d8abac2cabf0474'
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 pins={};git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; NATIVE LOADING SNAPSHOT INTEGRATED; TARGETED CPU AND WINDOWS HAL; SDK NRO NOT RUN','capture_profiles':profiles,'screenshot_profiles':png,'reference_map':mapped,'images':images,'native_prefix':prefix,'native_build':native,'native_objects_sha256':objects,'sdk':sdk,'clean_upstream_pins':pins,'protected_game_sha256':game,'protected_frozen_sha256':protected,'E579_NRO_preserved':sha(old_nro),'Switch':'NOT RUN','last_hardware_boundary':'ReadParams / ApplyGammaRamp before map constructor; unchanged by PC tests','remaining':['complete native/deko CGame::Init and first map/frame execution on Switch','hardware image/audio/HID/focus/F9/recovery validation','explicit error if completed snapshot is unavailable or Init changes capture extent'],'new_NRO':str(nro.relative_to(ROOT)),'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print('objects',len(objects),'PNG',108,'map_frames',len(images),'NRO',sha(nro),'bytes',nro.stat().st_size,'build_id',sdk['build_id'])
if __name__=='__main__':main()
