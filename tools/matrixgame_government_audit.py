"""Separate real government initialization proof from the unexecuted OnOpen/Galaxy transition."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e594-government-session'
def latest(folder):
 r=read(max(folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));assert r['status'].startswith('PASS');assert all(c['exit']==0 for c in r['commands']);return r
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 windows=latest(BASE/'windows-asan-depth');check_hashes(windows['sources_sha256']);check_hashes(windows['link_inputs_sha256']);assert sha(BASE/'windows-asan-depth/frun.exe')==windows['binary_sha256'];assert '/fsanitize=address' in windows['flags'] and '/UNDEBUG' in windows['flags'] and '/DNDEBUG' not in windows['flags'];assert 'ERROR: AddressSanitizer' not in windows['stdout']
 cpu={}
 for kind in ('asan','arm64'):
  r=latest(BASE/kind);check_hashes(r['source_sha256']);check_hashes(r['objects_sha256']);check_hashes(r['reused_objects_sha256']);check_hashes(r['libraries_sha256']);assert sha(BASE/kind/'government')==r['binary_sha256'];assert 'original_partial_tree_failure_rollback=PASS' in r['stdout'];assert 'borrowed_root_restore=PASS foreign_roots=REJECTED no_save_load=PASS CPU_LAYOUT_ONLY' in r['stdout'];cpu[kind]=r
 for r in (windows,*cpu.values()):
  assert r['stdout'].count('controls=395 original_Gov_constructor_config_layout=PASS real_callbacks=PASS duplicate_owner=REJECTED OnOpen_Galaxy=NOT_RUN')==2
 sdk=latest(BASE/'sdk');assert sdk['runtime']=='NOT RUN' and not sdk['strong_undefined'] and len(sdk['retained_API_symbols'])>=5;check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in {**sdk['artifacts'],**sdk['preserved_E592_artifacts']}.items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source')
 previous=read(ROOT/'reports/matrixgame-e2e593-rangers-caller-reset-evidence.json');check_hashes(previous['protected_game_sha256']);check_hashes(previous['protected_frozen_sha256'])
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');primary=font['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 archive=Path('D:/logs2.7z');expected_archive='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 external_archive={'path':str(archive),'previous_sha256':expected_archive,'status':'NOT VERIFIED; EXTERNAL FILE ABSENT'}
 if archive.is_file():
  actual=sha(archive);assert actual==expected_archive;external_archive.update(status='PASS; CURRENT SHA256 MATCHES',sha256=actual)
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PARTIAL; REAL GOVERNMENT TREE/LAYOUT/ROLLBACK PASS; ONOPEN/PLAYER/GALAXY TRANSITION NOT RUN','Windows_partial_ASan':windows,'CPU_runtime':cpu,'SDK':sdk,'external_logs2_archive':external_archive,'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E590_packages':previous['preserved_E590_packages'],'audit_sha256':sha(Path(__file__).resolve()),'remaining':['real Galaxy/Player/Planet and script runtime ownership','original TfGov::OnOpen battle caller/result/reward/history/screen transition','natural victory','native/deko FRun and hardware acceptance'],'new_NRO':False,'commit':False,'push':False}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status']);print(sdk['artifacts'])
if __name__=='__main__':main()
