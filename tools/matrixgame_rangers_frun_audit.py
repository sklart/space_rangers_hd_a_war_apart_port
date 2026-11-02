"""Audit actual FRun caller evidence without claiming a government/Galaxy return."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_presentation_overrides import render_inline
BASE=ROOT/'build/matrixgame/e2e593-rangers-caller-reset'
def latest(folder):return read(max(folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 reset=latest(BASE/'windows-asan-binding');frun=latest(BASE/'frun-windows-asan-gamma');sdk=latest(BASE/'sdk-configure')
 for r in (reset,frun,sdk):assert r['status'].startswith('PASS') and all(c['exit']==0 for c in r['commands'])
 for folder,r,binary in (('windows-asan-binding',reset,'legacy-reset.exe'),('frun-windows-asan-gamma',frun,'frun.exe')):
  check_hashes(r['sources_sha256']);assert sha(BASE/folder/binary)==r['binary_sha256'];assert '/fsanitize=address' in r['flags'] and '/UNDEBUG' in r['flags'] and '/DNDEBUG' not in r['flags'];assert 'ERROR: AddressSanitizer' not in r['stdout']
 check_hashes(reset['generated_headers_sha256']);check_hashes(reset['objects_sha256']);check_hashes(frun['link_inputs_sha256'])
 original=ROOT/'upstream/cpp/src/inline/Direct3D9.hpp';generated=BASE/'windows-asan-binding/source/inline/Direct3D9.hpp';assert generated.read_text(encoding='utf-8')==render_inline(original.read_text(encoding='utf-8'))
 main_source=ROOT/'build/matrixgame/e2e577-rangers-windows/source/GR_Main.cpp';assert sha(main_source)==frun['original_GR_Main_sha256']
 assert (BASE/'frun-windows-asan-gamma/GR_Main.cpp').read_text(encoding='utf-8')==main_source.read_text(encoding='utf-8')+'\n'+(ROOT/'tests/matrixgame_rangers_frun_window.inc').read_text(encoding='utf-8')
 assert sha(ROOT/'build/matrixgame/e2e577-rangers-windows/objects/original/Robot.obj')==frun['original_Robot_object_sha256']
 for marker in ('original_caller_stats=PASS sound_music_restore=PASS original_GI_invalidation=PASS cwd_loose_root_cleanup=PASS Galaxy_return=NOT_RUN','actual FRun caller target/viewport readback 800x600; device/timer leases empty','RANGERS_CALLER_GAMMA configured_before_battle_equals_after_FRun=PASS real_SDK_readback=PASS'):
  assert frun['stdout'].count(marker)==1,marker
 assert 'real_HAL_Reset=PASS SDK_in_out_copy=PASS unaligned_56byte_canaries=PASS' in reset['stdout'] and 'retained_DEFAULT_failure=8876086c actual_release_retry=PASS' in reset['stdout']
 assert 'explicit_token_full_window=PASS token_copyback=PASS wrong_token_unchanged=PASS duplicate_binding=REJECTED scope_release=PASS' in reset['stdout']
 metadata=read(BASE/'sdk-configure/native-diagnostic-build.json');assert sha(BASE/'sdk-configure/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk-configure/source')
 previous=read(ROOT/'reports/matrixgame-e2e592-native-rangers-interface-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 for name,info in sdk['preserved_E592_artifacts'].items():assert sha(ROOT/name)==info['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash'];assert sha(Path('D:/logs2.7z'))=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 r={'status':'PARTIAL; ORIGINAL FRUN FULL MISSION/RESULT/SDK RESET AND REAL BASE GI INVALIDATION PASS; TF GOV/GALAXY NOT RUN','Windows_partial_ASan_FRun':frun,'Windows_ASan_original_inline_Reset':reset,'SDK_configure_only':sdk,'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E590_packages':previous['preserved_E590_packages'],'audit_sha256':sha(Path(__file__).resolve()),'remaining':['real in-memory Galaxy/Player and government GI-tree/renderer session','TfGov::OnOpen rewards/history and galactic screen transition','natural victory','native/deko FRun renderer boundary and hardware execution'],'new_NRO':False,'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(r,stream,indent=2);stream.write('\n')
 print(r['status'])
 print('\n'.join(l for l in frun['stdout'].splitlines() if l.startswith('RANGERS_FRUN')))
if __name__=='__main__':main()
