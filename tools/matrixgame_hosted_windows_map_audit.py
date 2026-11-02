"""Verify E579 real PC map execution, scoped original textures and SDK packaging."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from matrixgame_screenshot_png_verify import decode
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map'
def latest(folder,pattern,prefix):
 for p in sorted(folder.glob(pattern),key=lambda p:p.stat().st_mtime_ns,reverse=True):
  value=read(p)
  if value['status'].startswith(prefix):return p,value
 raise AssertionError('missing completed PASS: '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 map_path,mapped=latest(BASE,'result*.json','PASS; REAL WINDOWS HAL')
 build_path,native=latest(BASE/'native-build','build-result*.json','PASS; FRESH')
 prefix_path,prefix=latest(BASE/'native-build/prefix','result*.json','PASS; WINDOWS REAL HOSTED')
 for value in (mapped,native,prefix):assert all(c['exit']==0 for c in value['commands'])
 for key in ('link_inputs_sha256','sources_sha256','outputs_sha256'):check_hashes(mapped[key])
 assert sha(BASE/'hosted-map.exe')==mapped['binary_sha256']
 assert mapped['stdout'].count('full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS')==2
 assert mapped['stdout'].count('HOSTED_MAP final_host_cleanup=PASS')==2 and mapped['stdout'].count('readback_present=PASS')==8
 assert mapped['stdout'].count('actual gamma/window readback restored; device/timer leases empty')==2
 assert mapped['stdout'].count('original_transition_takt_draw_frames=30 completed=PASS')==2
 pngs=sorted((BASE/'runtime/output').glob('map-frame-*.png'));assert len(pngs)==8
 images={}
 for p in pngs:
  width,height,rgb=decode(p.read_bytes(),allow_alpha=True);assert (width,height)==(800,600)
  count=len({rgb[i:i+3] for i in range(0,len(rgb),3)});assert count>=64
  images[str(p.relative_to(ROOT))]={'sha256':sha(p),'rgb_sha256':hashlib.sha256(rgb).hexdigest(),'size':[width,height],'distinct_rgb':count}
 journals={};completed=0
 for p in (BASE/'runtime/output/journals').rglob('*.jsonl'):
  lines=[json.loads(line) for line in p.read_text().splitlines() if line]
  if lines and lines[0]['source_sha256']==mapped['composition_source_sha256']:
   assert any(row['event']=='ok' and row['operation']=='B.CGame::Init' for row in lines)
   assert not any(row['event']=='fail' for row in lines);journals[str(p.relative_to(ROOT))]=sha(p);completed+=1
 assert completed==2
 check_hashes(native['archives_sha256']);check_hashes(native['image_inputs_sha256'])
 assert native['generated']==read(BASE/'native-build/native/source/native-engine-source.json')
 check_hashes(native['generated']['files_sha256'],BASE/'native-build/native/source')
 commands=read(BASE/'native-build/native/compile_commands.json');objects={};sources={}
 targets=('matrixgame-native-cache','matrixgame-native-vector-object','matrixgame-native-navigation','matrixgame-native-core','matrixgame-material-combiner')
 for entry in commands:
  if not any('/'+name+'.dir/' in '/'+entry['output'].replace('\\','/') for name in targets):continue
  assert '/DNDEBUG' not in entry['command'] and '/UNDEBUG' in entry['command'] and '-MT' in entry['command']
  source=Path(entry['file']);obj=Path(entry['directory'])/entry['output'];sources[str(source.relative_to(ROOT))]=sha(source);objects[str(obj.relative_to(ROOT))]=sha(obj)
 assert len(objects)==159
 factory={}
 for kind in ('windows','host','arm64'):
  p,record=latest(BASE/('factory-'+kind),'result*.json','PASS; ACTUAL ORIGINAL TEXTURE')
  assert all(c['exit']==0 for c in record['commands']);check_hashes(record['source_sha256'])
  if kind=='windows':check_hashes(record['link_inputs_sha256'])
  factory[kind]=record
 assert prefix['stdout'].count('original Init exception: HOSTED_NATIVE_CAPS_BOUNDARY')==2
 check_hashes(prefix['source_sha256']);check_hashes(prefix['link_inputs_sha256'])
 sdk=read(BASE/'sdk/result.json');assert sdk['status'].startswith('PASS') and all(c['exit']==0 for c in sdk['commands'])
 assert not sdk['strong_undefined'];check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_sha256'])
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256']
 check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source')
 assert metadata['rangers_host']['enabled'];check_hashes(metadata['rangers_host']['archives_sha256'])
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 nro=BASE/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';nacp=BASE/'sdk/diagnostic-nro/control.nacp'
 verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes())
 for filename in ('MatrixTerSurface.cpp','MatrixMapStatic.cpp'):
  path=BASE/'sdk/source/MatrixGame/src'/filename;text=path.read_text();assert 'memcopy_back_dword(' not in text and 'std::memmove(' in text
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 game=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/host/result.json')['protected_game_sha256'];check_hashes(game)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 previous_nro=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(previous_nro)=='9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356'
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 pins={};git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; REAL WINDOWS HOSTED FULL INIT MAP EIGHT FRAMES TWO CLEANUPS; FACTORY FIX AND POINTER QUEUES; SDK NRO NOT RUN',
  'map':mapped,'native_build':native,'native_objects_sha256':objects,'native_sources_sha256':sources,'factory_profiles':factory,'early_prefix':prefix,'sdk':sdk,
  'images':images,'matching_completed_journals':journals,'clean_upstream_pins':pins,'protected_game_sha256':game,'protected_frozen_sha256':protected,'E575_NRO_preserved':sha(previous_nro),
  'known_native_gap':'Non-Windows transition CaptureDC/Bitmap screen-capture operations still explicitly Unsupported; real Rangers callback binding makes this branch mandatory. Needs genuine native framebuffer capture provider, not a fake loading image.',
  'remaining':['native transition framebuffer capture integration','combined NRO complete Init/map/frame execution','Switch image/audio/HID/focus/F9/recovery'],
  'Switch':'NOT RUN','new_NRO':str(nro.relative_to(ROOT)),'commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print('Windows images',len(images),'SDK NRO',sha(nro));print('known native gap',result['known_native_gap'])
if __name__=='__main__':main()
