"""Verify original config differential, real borrowed services and DAT cleanup."""
import argparse
import json
import subprocess
from pathlib import Path
from e2e_dat_cleanup import apply as dat_cleanup
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e573-rangers-data'
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 profiles={name:read(BASE/(name+'-final')/'result.json') for name in ('host','asan','sdk')}
 baseline=read(BASE/'host/result.json');assert baseline['status'].startswith('PASS')
 for file,digest in baseline['binaries'].items():assert sha(BASE/'host'/file)==digest
 for name,result in profiles.items():
  assert result['status'].startswith('PASS') and all(c['exit']==0 for c in result['commands']),name
  check_hashes(result['source_sha256']);check_hashes(result['dat_cleanup_inputs_sha256']);check_hashes(result['dat_cleanup_outputs_sha256'])
  for source in result['dat_cleanup_outputs_sha256']:
   generated=ROOT/source;original=next(ROOT/p for p in result['dat_cleanup_inputs_sha256'] if Path(p).name==generated.name)
   assert generated.read_text(encoding='utf-8')==dat_cleanup(original.read_text(encoding='utf-8'),generated.name)
  if name=='sdk':continue
  for key in ('reused_objects_sha256','new_objects_sha256','protected_before_after'):check_hashes(result[key])
  assert len(result['reused_objects_sha256'])==332
  for binary,digest in result['binaries'].items():assert sha(BASE/(name+'-final')/binary)==digest
  for case,values in result['comparisons'].items():
   assert values['owned']==values['reference']==baseline['comparisons'][case]['reference']
   for mode in ('reference','owned'):
    for file,digest in values[mode].items():assert sha(BASE/(name+'-final')/case/mode/file)==digest
  assert 'OWNED_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS' in result['owned_host_stdout']
  for case in ('package-error','language-error'):
   log=(BASE/(name+'-final')/('run-'+case+'.log')).read_text()
   assert 'cleanup=PASS state_restore=PASS' in log
 host=profiles['host'];asan=profiles['asan'];assert asan['new_TUs_instrumented'] and not asan['original_dependencies_instrumented']
 assert host['comparisons']['mod']['owned']['main.bin']!=host['comparisons']['russian']['owned']['main.bin']
 assert host['comparisons']['mod']['owned']['cache.bin']!=host['comparisons']['russian']['owned']['cache.bin']
 for name in ('main.bin','language.bin','cache.bin'):
  assert host['comparisons']['skip']['owned'][name]==host['comparisons']['russian']['owned'][name]
 sdk=profiles['sdk'];assert not sdk['strong_undefined'];check_hashes(sdk['link_inputs_sha256']);check_hashes(sdk['dependency_objects_sha256']);check_hashes(sdk['protected_E572_sha256'])
 for file,value in sdk['artifacts'].items():assert sha(ROOT/file)==value['sha256']
 symbols=(BASE/'sdk-final/symbols.log').read_text();assert all(name in symbols for name in sdk['required_linked_symbols'])
 assert 'CGame::Init(' not in symbols and 'AArch64' in sdk['elf_header']
 leak=BASE/'asan/run-language-error.log';assert 'LeakSanitizer: detected memory leaks' in leak.read_text() and '76 byte(s)' in leak.read_text()
 regression=read(BASE/'dat-cleanup-asan/result.json');assert regression['status'].startswith('PASS')
 check_hashes(regression['source_sha256']);check_hashes(regression['reused_objects_sha256']);assert all(c['exit']==0 for c in regression['commands'])
 assert sha(BASE/'dat-cleanup-asan/probe.o')==regression['probe_object_sha256'] and sha(BASE/'dat-cleanup-asan/dat-cleanup')==regression['binary_sha256']
 assert 'missing_open=2 short_read=2 exception=PASS handles_closed=PASS' in regression['stdout']
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
 result={'status':'PASS; ORIGINAL DATA DIFFERENTIAL + BORROWED FULL16 + DAT CLEANUP; SDK LINK ONLY','profiles':profiles,'dat_cleanup_regression':regression,
  'baseline_leak_log_sha256':sha(leak),'baseline_original_host_binary_sha256':baseline['binaries'],'upstream_pins_clean':pins,'protected_verified':len(protected)+2,'E568_NRO_preserved':sha(nro),
  'scope':'Five exact original GR_Main package/config differential cases including real SR2LoadingScreen mod; constructor failure/retry and state restoration; borrowed full16 sound/AFT/music host SDL dummy. ASan instruments nine new TUs per final binary including both original DAT loaders; 329 original dependencies not instrumented. Full SDK link, NOT RUN.',
  'remaining':['native runtime composition with original Init/Deinit and package bridge','audio-disabled settings integration','native Init/map and updated deko3d diagnostic NRO','ARM64/QEMU and Windows execution of new data/full16 probes','user Switch graphics/audio/input/focus/F9/recovery validation'],
  'new_NRO':False,'Switch':'NOT RUN','commit':False,'push':False,
  'audit_sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in (Path(__file__).resolve(),ROOT/'tools/matrixgame_dat_cleanup_checks.py')}}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print(json.dumps(sdk['artifacts'],indent=2))
if __name__=='__main__':main()
