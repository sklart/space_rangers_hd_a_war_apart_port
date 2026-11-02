"""Verify first real ARM64/QEMU hosted execution and preserve prior SDK NRO."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e576-rangers-arm64'
def local(name):
 return Path(name.replace('/mnt/d/','D:/')) if name.startswith('/mnt/d/') else ROOT/name.replace('\\','/')
def check(values):
 for name,digest in values.items():assert sha(local(name))==digest,name
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 deps=read(BASE/'deps/result.json');core=read(BASE/'result.json');extra=read(BASE/'extra/result.json')
 for name,result in (('deps',deps),('core',core),('extra',extra)):
  assert result['status'].startswith('PASS'),name
  assert all(c['exit']==0 for c in result['commands']),name
 check(deps['libraries_sha256']);check(deps['headers_sha256'])
 assert sha(ROOT/'tools/matrixgame_rangers_arm64_deps.py')==deps['script_sha256']
 for archive in deps['archives']:assert sha(local(archive['path']))==archive['sha256']
 config=(BASE/'deps/prefix/include/SDL2/SDL_config.h').read_text();assert '#define SDL_AUDIO_DRIVER_DUMMY 1' in config
 for key in ('effective_sources_sha256','objects_sha256','libraries_sha256','protected_sha256','okgf_original_files_sha256'):check(core[key])
 assert len(core['objects_sha256'])==339 and sha(BASE/'rangers-host-arm64')==core['binary_sha256']
 assert sha(ROOT/'tools/matrixgame_rangers_arm64_checks.py')==core['script_sha256']
 assert 'AArch64' in core['elf_header'] and '-UNDEBUG' in core['flags']
 for effects in (0,1):
  for music in (0,1):assert f'HOST_SESSION effects={effects} music={music} full16=PASS teardown=PASS' in core['stdout']
 assert 'original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS' in core['stdout']
 header_inputs={}
 for obj in core['objects_sha256']:
  stamp=read(local(obj).with_suffix('.stamp.json'));assert stamp['object_sha256']==core['objects_sha256'][obj]
  check(stamp['headers_sha256']);header_inputs.update(stamp['headers_sha256'])
 assert (BASE/'okgf-source/CMakeLists.txt').read_text()==(ROOT/'upstream/okgf/CMakeLists.txt').read_text().replace('add_library(okgf SHARED','add_library(okgf STATIC',1)
 for file,digest in core['okgf_original_files_sha256'].items():
  source=ROOT/file;relative=source.relative_to(ROOT/'upstream/okgf')
  if relative!=Path('CMakeLists.txt'):assert sha(BASE/'okgf-source'/relative)==digest
 for key in ('source_sha256','reused_objects_sha256','native_objects_sha256','native_sources_sha256','native_archives_sha256','native_fixture_sha256','new_objects_sha256','protected_game_sha256'):check(extra[key])
 for file,digest in extra['binaries'].items():assert sha(BASE/'extra'/file)==digest
 assert 'exact_original_pairs=24 crop=1 automatic=1 error_cleanup=PASS' in extra['text_stdout']
 pixels=BASE/'extra/text-runtime/pixels.bin';assert sha(pixels)==extra['pixel_sha256']==extra['reference_pixel_sha256'] and pixels.stat().st_size==24*200*64*4
 assert 'original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS' in extra['prefix_stdout']
 assert extra['prefix_stdout'].count('original Init exception: HOSTED_NATIVE_CAPS_BOUNDARY')==2
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 latest=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(latest)=='9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS; REAL ARM64/QEMU HOST MODES, AFT PIXELS AND HOSTED ORIGINAL INIT ROLLBACK','dependencies':deps,'core':core,'extra':extra,
  'header_inputs_verified':len(header_inputs),'upstream_pins_clean':pins,'latest_E575_NRO_preserved':sha(latest),
  'scope':'First Linux AArch64 build of all331 effective original units +7 native units +probe. QEMU executes real original sound/AFT/music callbacks in four modes with true SDL dummy backend. Extra AFT pixels match x86-64 exactly; actual early original native Init uses real host callbacks through two unsupported-caps rollback cycles. No ARM ASan, no GPU, no audible output or full native Init/map, no Switch execution.',
  'remaining':['Windows reference execution of hosted chain','complete native Init/map and first map frames on combined NRO','Switch image/audio/HID/focus/F9/recovery validation'],
  'new_NRO':False,'Switch':'NOT RUN','commit':False,'push':False,'audit_source_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print('ARM64 objects',len(core['objects_sha256']),'pixels',extra['pixel_sha256']);print('E575 NRO preserved',sha(latest))
if __name__=='__main__':main()
