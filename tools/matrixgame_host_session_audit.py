"""Audit owning host audio modes and real callbacks through native Init rollback."""
import argparse
import json
import re
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e574-rangers-host-session'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 profiles={name:read(BASE/name/'result.json') for name in ('host','asan','sdk')}
 for name,result in profiles.items():
  assert result['status'].startswith('PASS') and all(c['exit']==0 for c in result['commands']),name
  check_hashes(result['source_sha256'])
  if name=='sdk':continue
  for key in ('reused_objects_sha256','new_objects_sha256','protected_game_sha256'):check_hashes(result[key])
  assert len(result['reused_objects_sha256'])==335 and sha(BASE/name/'rangers-host-session')==result['binary_sha256']
  for effects in (0,1):
   for music in (0,1):assert f'HOST_SESSION effects={effects} music={music} full16=PASS teardown=PASS' in result['stdout']
  assert 'modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS SDL_DUMMY_ONLY NO_NATIVE_INIT NO_SWITCH' in result['stdout']
 assert profiles['asan']['new_TUs_instrumented'] and not profiles['asan']['original_dependencies_instrumented']
 assert '-fsanitize=address,undefined' in profiles['asan']['commands'][0]['command']
 probe=(ROOT/'tests/probe_matrixgame_rangers_host_session.cpp').read_text();assert len(set(re.findall(r'table\.(m_\w+)\(',probe)))==16
 sdk=profiles['sdk'];assert not sdk['strong_undefined'];check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['protected_E573_sha256'])
 for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
 symbols=(BASE/'sdk/symbols.log').read_text();assert all(name in symbols for name in sdk['required_linked_symbols'])
 assert 'CGame::Init(' not in symbols and 'AArch64' in sdk['elf_header']
 prefix=read(BASE/'native-prefix-final/result.json');assert prefix['status'].startswith('PASS') and all(c['exit']==0 for c in prefix['commands'])
 for key in ('source_sha256','objects_sha256','archives_sha256','native_fixture_sha256'):check_hashes(prefix[key])
 assert sha(BASE/'native-prefix-final/probe.o')==prefix['probe_object_sha256'] and sha(BASE/'native-prefix-final/hosted-native-prefix')==prefix['binary_sha256']
 assert 'original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS NO_GPU NO_FULL_INIT NO_SWITCH' in prefix['stdout']
 assert prefix['stdout'].count('original Init exception: HOSTED_NATIVE_CAPS_BOUNDARY')==2
 native_root=Path(prefix['native_read_root'])
 for name in ('data.txt','iface.txt'):assert sha(native_root/'CFG/robots'/name)==sha(ROOT/'upstream/matrixgame/MatrixGame/CFG/robots'/name)
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 nro=ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(nro)=='d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
  assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; OWNING HOST FOUR MODES + REAL HOSTED NATIVE INIT ROLLBACK; FULL INIT OPEN','profiles':profiles,'native_prefix':prefix,
  'scope':'All16 actual callbacks in four audio modes, real data/audio/music owners, dummy SDL. Ten new host TUs instrumented, 329 original Rangers dependencies not instrumented. Early actual original CGame::Init with real callbacks and ASan/UBSan cleanup, deliberate unsupported Device::ReadCaps boundary, two cycles. SDK full host ELF linked, NOT RUN.',
  'upstream_pins_clean':pins,'protected_verified':len(protected)+2,'E568_NRO_preserved':sha(nro),'new_NRO':False,'Switch':'NOT RUN','commit':False,'push':False,
  'remaining':['integrate owning host into standard diagnostic entry and link native/deko + Rangers together','explicit Matrix text-config root and Rangers install root setup','complete native Init/map and updated separate NRO','ARM64/QEMU and Windows execution of new hosted chain','user Switch graphics/audio/input/focus/F9/recovery validation'],
  'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(json.dumps(sdk['artifacts'],indent=2))
if __name__=='__main__':main()
