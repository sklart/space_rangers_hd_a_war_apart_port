"""Verify all four real Robot methods; preserve the separate Galaxy/hardware gates."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e592-native-rangers-interface'
def latest(folder):
 p=max(folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns);r=read(p);assert r['status'].startswith('PASS');return r
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 runtime=latest(BASE/'windows-asan-fixed');assert all(c['exit']==0 for c in runtime['commands']);check_hashes(runtime['sources_sha256']);check_hashes(runtime['link_inputs_sha256']);assert sha(BASE/'windows-asan-fixed/rangers-run.exe')==runtime['binary_sha256']
 assert '/fsanitize=address' in runtime['flags'] and '/UNDEBUG' in runtime['flags'] and '/DNDEBUG' not in runtime['flags']
 for marker in ('ROBOT_INTERFACE default_host_callbacks_regression=PASS','all_four_real_slots=PASS invalid_Initialize_rollback=PASS callbacks_readonly=PASS global_install=PASS','original_FinalizeRobotRuntime=PASS Run_after_Finalize_rejected=PASS reinitialize=PASS duplicate_Initialize_rejected=PASS','caller_settings_nonnull=PASS caller_bytes_unchanged=PASS','typed_Run=PASS original_stats=PASS owner_cleanup_before_return=PASS host_alive=PASS legacy_tokens_not_pointer_casts=PASS','actual gamma/window readback restored; device/timer leases empty'):
  assert runtime['stdout'].count(marker)==1,marker
 rows=[dict(p.split('=',1) for p in l.split()[1:]) for l in runtime['stdout'].splitlines() if l.startswith('ROBOT_INTERFACE real_support_query=')];assert len(rows)==1
 assert int(rows[0]['textures'])>=2 and int(rows[0]['width'])>=2048 and int(rows[0]['streams'])>=1
 assert 'ERROR: AddressSanitizer' not in runtime['stdout']
 policies={};compiles={}
 for kind in ('windows','host','arm64'):
  r=latest(BASE/'policy'/kind);assert all(c['exit']==0 for c in r['commands']);check_hashes(r['source_sha256']);assert sha(BASE/'policy'/kind/('support.exe' if kind=='windows' else 'support'))==r['binary_sha256'];assert 'original_code_order=PASS texture_width_boundary=PASS stream_boundary=PASS query_failure=PASS CPU_POLICY_ONLY' in r['stdout'];policies[kind]=r
 for kind in ('host','arm64'):
  r=latest(BASE/('compile-'+kind));assert r['runtime']=='NOT RUN' and all(c['exit']==0 for c in r['commands']);check_hashes(r['source_sha256']);check_hashes(r['objects_sha256']);assert len(r['objects_sha256'])==3;compiles[kind]=r
 sdk=latest(BASE/'sdk');assert not sdk['strong_undefined'] and sdk['runtime']=='NOT RUN' and len(sdk['retained_API_symbols'])>=6 and all(c['exit']==0 for c in sdk['commands']);check_hashes(sdk['link_inputs_sha256'])
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source');check_hashes(metadata['rangers_host']['archives_sha256'])
 before=read(ROOT/'build/matrixgame/e2e591-native-rangers-run/sdk/native-diagnostic-build.json');assert metadata['generated_original_sha256']==before['generated_original_sha256']
 source=(ROOT/'port/matrixgame/native-engine/native_texture.cpp').read_text(encoding='utf-8');assert 'w>8192 || h>8192' in source
 source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text(encoding='utf-8');assert 'if(stream || current.buffer!=buffer' in source
 previous=read(ROOT/'reports/matrixgame-e2e591-native-rangers-run-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 for package in previous['preserved_E590_packages'].values():assert sha(ROOT/package['path'])==package['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash'];assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 extra=[ROOT/'port/matrixgame/native-engine'/n for n in ('native_rangers_interface.hpp','native_rangers_support.hpp')]+[ROOT/'port/matrixgame/native/rangers_host_session.hpp',Path(__file__).resolve()]
 r={'status':'PARTIAL; COMPLETE REAL NATIVE ROBOT INTERFACE LIFECYCLE AND SUPPORT PASS; ORIGINAL FINALIZE CALLER AND FULL MISSION PASS; GALAXY RETURN NOT RUN',
  'Windows_partial_ASan':runtime,'actual_Windows_caps':rows,'CPU_support_policies':policies,'cross_compile_only':compiles,'SDK':sdk,'generated_originals_unchanged':True,
  'extra_inputs_sha256':{str(p.relative_to(ROOT)):sha(p) for p in extra},'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E590_packages':previous['preserved_E590_packages'],
  'native_Deko_Support_runtime':'NOT RUN; REAL SDK QUERY BRANCH COMPILED/LINKED ONLY','original_InitializeRobotRuntime_DLL_loader':'NOT USED; NATIVE INSTALL OWNS REAL CALLBACK INITIALIZATION','original_FinalizeRobotRuntime':'EXECUTED','FRun_fGov_Galaxy_transition':'NOT RUN',
  'remaining':['real in-memory Galaxy/Player/government GI-tree and renderer session','original FRun and reward/history/galaxy transition','victory and native/deko hardware execution'],'new_NRO':False,'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(r,stream,indent=2);stream.write('\n')
 print(r['status']);print(rows);print(sdk['artifacts'])
if __name__=='__main__':main()
