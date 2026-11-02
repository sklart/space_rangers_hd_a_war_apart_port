"""Read-only evidence audit of the executed Windows hosted services gate."""
import argparse
import json
import subprocess
from pathlib import Path
from e2e_runtime_overrides import prepare_runtime_overlay
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows'
def latest_pass(folder):
 candidates=sorted(folder.glob('attempt-*/result.json'),reverse=True)
 for path in candidates:
  result=read(path)
  if result['status'].startswith('PASS'):
   assert all(c['exit']==0 for c in result['commands']);return path,result
 raise AssertionError('no completed PASS: '+str(folder))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 core=read(BASE/'result.json');deps=read(BASE/'deps/result.json')
 for result in (core,deps):assert result['status'].startswith('PASS') and all(c['exit']==0 for c in result['commands'])
 for key in ('effective_inputs_sha256','source_sha256','headers_sha256','objects_sha256','protected_sha256'):check_hashes(core[key])
 for key in ('libraries_sha256','headers_sha256'):check_hashes(deps[key])
 assert len(core['objects_sha256'])==339 and sha(BASE/'rangers-host-windows.exe')==core['binary_sha256']
 assert '/UNDEBUG' in core['flags'] and '/MT' in core['flags']
 assert sha(ROOT/'tools/matrixgame_rangers_windows_checks.py')==core['script_sha256']
 for effects in (0,1):
  for music in (0,1):assert f'HOST_SESSION effects={effects} music={music} full16=PASS teardown=PASS' in core['stdout']
 assert 'original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS' in core['stdout']
 text_path,text=latest_pass(BASE/'text');sync_path,sync=latest_pass(BASE/'sync');perf_path,perf=latest_pass(BASE/'perf-clock')
 check_hashes(text['source_sha256']);check_hashes(text['reused_objects_sha256']);check_hashes(sync['source_sha256']);check_hashes(perf['sources_sha256'])
 pixels=text_path.parent/'runtime/pixels.bin';reference=ROOT/'build/matrixgame/e2e570-rangers-text/host/runtime/pixels.bin'
 assert pixels.stat().st_size==1228800 and sha(pixels)==sha(reference)==text['pixels_sha256']==text['reference_pixels_sha256']
 assert sha(text_path.parent/'rangers-text.exe')==text['binary_sha256']
 assert sync['overlay_sha256']==sha(BASE/'runtime-overlay/sync.hpp') and 'sdk_size=40 sdk_align=8' in sync['stdout']
 assert sha(perf_path.parent/'perf-clock.exe')==perf['binary_sha256']
 assert perf['anchors'] and all(perf['window_epoch_ms'][0]<=int(row[2])<=perf['window_epoch_ms'][1] for row in perf['anchors'])
 # Generating the ordinary overlay must retain all previous Linux/SDK bytes.
 parity=BASE/'audit-default-runtime-parity';prepare_runtime_overlay(ROOT/'upstream/cpp/runtime',parity)
 default=ROOT/'port/switch/build/e2e-game-perf515/runtime-overlay'
 for name in ('windows.hpp','float_text.hpp','graphics.hpp','locale.hpp','e2e510_heap_reserve.hpp'):assert sha(parity/name)==sha(default/name),name
 game=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/host/result.json')['protected_game_sha256'];check_hashes(game)
 previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
 protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
 protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
 primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
 assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
 latest=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(latest)=='9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356'
 assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 pins={};git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 deltas=sorted((BASE/'delta').glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);delta=next(read(p) for p in deltas if read(p)['status'].startswith('PASS'))
 assert all(c['exit']==0 for c in delta['commands']);check_hashes(delta['source_sha256']);check_hashes(delta['objects_sha256'])
 result={'status':'PASS; WINDOWS MSVC REAL HOST FOUR MODES, EXACT AFT PIXELS, NATIVE SYNC ABI AND CLOCKS',
  'dependencies':deps,'core':core,'text':text,'sync':sync,'perf':perf,'limited_delta':delta,
  'protected_game_sha256':game,'protected_frozen_sha256':protected,'clean_upstream_pins':pins,'E575_NRO_preserved':sha(latest),
  'reference_platform':'Windows x64 LLP64, MSVC 19.44, NATIVE math, genuine SDL dummy audio; no audible/device proof',
  'remaining':['Windows native CGame::Init hosted prefix execution','complete native Init/map and first map frames','Switch graphics/audio/HID/focus/F9/recovery'],
  'full_suites_rerun':False,'new_NRO':False,'Switch':'NOT RUN','commit':False,'push':False,'audit_script_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status']);print('objects',len(core['objects_sha256']),'pixels',text['pixels_sha256']);print('NRO preserved',sha(latest))
if __name__=='__main__':main()
