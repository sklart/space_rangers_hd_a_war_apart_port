"""Record campaign execution and a real failed leak gate without upgrading it to PASS."""
import argparse,json,re,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members
BASE=ROOT/'build/matrixgame/e2e598-campaign'
def latest(folder,attempts=False):
 path=max(folder.glob('*/result.json') if attempts else folder.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns)
 return path,read(path)
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 win_path,windows=latest(BASE/'windows-asan',True);assert windows['status'].startswith('PASS');check_hashes(windows['sources_sha256']);check_hashes(windows['link_inputs_sha256']);assert sha(win_path.parent/'campaign.exe')==windows['binary_sha256']
 arm_path,arm=latest(BASE/'arm64',True);assert arm['status'].startswith('PASS')
 for key in ('sources_sha256','objects_sha256','reused_objects_sha256','libraries_sha256'):check_hashes(arm[key])
 assert sha(ROOT/arm['binary'])==arm['binary_sha256']
 asan_path,asan=latest(BASE/'asan',True);assert asan['status']=='INCOMPLETE' and asan['commands'][-1]['label']=='runtime' and asan['commands'][-1]['exit']==1
 assert all(c['exit']==0 for c in asan['commands'][:-1]);asan_log=ROOT/asan['commands'][-1]['log'];log=asan_log.read_text(encoding='utf-8');leaks=re.search(r'SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)',log);assert leaks
 for text in (windows['stdout'],arm['stdout'],log):
  assert 'original_generation_complete=PASS stage=8 stars=73' in text and 'real_Player_Star_HomePlanet=PASS duplicate_owner=REJECTED no_save_load=PASS' in text
  assert 'original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN' in text
 dep_path,dep=latest(BASE/'sdk-dependency');assert dep['status'].startswith('PASS');check_hashes(dep['source_sha256']);check_hashes(dep['dependency_objects_sha256']);assert dep['changed_members']==['fGameSettings.o'];assert sha(ROOT/dep['archive_path'])==dep['dependency_archive_sha256'] and members(ROOT/dep['archive_path'])==dep['archive_members_sha256']
 sdk_path,sdk=latest(BASE/'sdk');assert sdk['status'].startswith('PASS') and not sdk['strong_undefined'] and sdk['runtime']=='NOT RUN';check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in {**sdk['artifacts'],**sdk['preserved_E592_artifacts']}.items():assert sha(ROOT/name)==info['sha256']
 meta=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256'];check_hashes(meta['own_source_sha256']);check_hashes(meta['generated_original_sha256'],BASE/'sdk/source');check_hashes(meta['rangers_host']['archives_sha256']);assert meta['rangers_host']['dependency_manifest_sha256']==sha(dep_path)
 previous=read(ROOT/'reports/matrixgame-e2e597-global-ui-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(previous[key])
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 primary=previous['primary_NRO'];assert sha(Path(primary['path']))==primary['sha256'];font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip();assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 archive=Path('D:/logs2.7z');external={'path':str(archive),'status':'NOT VERIFIED; EXTERNAL FILE ABSENT'}
 if archive.is_file():
  external.update(status='PASS; CURRENT SHA256 MATCHES',sha256=sha(archive));assert external['sha256']=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 evidence={'status':'PARTIAL; ORIGINAL CAMPAIGN EXECUTED; HOST LEAKSANITIZER FAILED; BATTLE/GOV RETURN NOT RUN','Windows_partial_ASan':windows,'ARM64_QEMU':arm,'host_ASan_UBSan':{'status':'FAIL; LEAKSANITIZER; FUNCTIONAL STAGE8 REACHED','result':asan,'log':str(asan_log.relative_to(ROOT)),'log_sha256':sha(asan_log),'leaked_bytes':int(leaks[1]),'leaked_allocations':int(leaks[2])},'SDK_dependency':dep,'SDK':sdk,'clean_upstream_pins':pins,'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],'preserved_E590_packages':previous['preserved_E590_packages'],'primary_NRO':primary,'external_logs2_archive':external,'new_NRO':False,'commit':False,'push':False,'remaining':['original campaign leaks in constellation edge rebuild/planet relation containers and further ownership audit','original Gov OnOpen battle result/rewards/history/return','natural victory','native/deko FRun runtime and user hardware acceptance'],'audit_sha256':sha(Path(__file__).resolve())}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status']);print(evidence['host_ASan_UBSan']['leaked_bytes'],evidence['host_ASan_UBSan']['leaked_allocations']);print(sdk['artifacts'])
if __name__=='__main__':main()
