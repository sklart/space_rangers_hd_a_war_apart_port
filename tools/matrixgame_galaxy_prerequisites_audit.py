"""Keep original Galaxy construction proof distinct from generated Player/campaign proof."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e596-galaxy-prerequisites'
def latest(folder,attempts=False):
 paths=folder.glob('*/result.json') if attempts else folder.glob('result*.json')
 path=max(paths,key=lambda p:p.stat().st_mtime_ns);r=read(path)
 assert r['status'].startswith('PASS'),path
 assert all(c['exit']==0 for c in r['commands']);return r
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
 runtimes={k:latest(BASE/k,True) for k in ('windows-asan','asan','arm64')}
 for kind,r in runtimes.items():
  assert r['stdout'].count('controls=341 original_Hangar_config_layout=PASS original_TCCInterface=PASS original_TGalaxy_create_destroy=PASS integrity_operations=UNMODIFIED campaign_Player=NOT_GENERATED')==2
  assert 'partial_layout_rollback=PASS foreign_TCC_roots_duplicate=REJECTED original_cleanup_borrowed_roots=PASS no_save_load=PASS campaign_generation=NOT_RUN' in r['stdout']
  assert 'ERROR: AddressSanitizer' not in r['stdout'] and 'runtime error:' not in r['stdout']
  for key in ('sources_sha256','objects_sha256','link_inputs_sha256','reused_objects_sha256','libraries_sha256'):check_hashes(r.get(key,{}))
  assert sha(ROOT/r['binary'])==r['binary_sha256'] and r['campaign_generation']=='NOT RUN'
 assert '/fsanitize=address' in runtimes['windows-asan']['flags'] and '/UNDEBUG' in runtimes['windows-asan']['flags']
 sdk=latest(BASE/'sdk');assert sdk['runtime']=='NOT RUN' and not sdk['strong_undefined'] and len(sdk['retained_API_symbols'])>=5
 check_hashes(sdk['new_link_inputs_sha256']);check_hashes(sdk['reused_E592_link_inputs_sha256'])
 for name,info in {**sdk['artifacts'],**sdk['preserved_E592_artifacts']}.items():assert sha(ROOT/name)==info['sha256']
 metadata=read(BASE/'sdk/native-diagnostic-build.json');assert sha(BASE/'sdk/native-diagnostic-build.json')==sdk['metadata_sha256']
 check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source')
 previous=read(ROOT/'reports/matrixgame-e2e595-script-session-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(previous[key])
 for info in previous['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 primary=previous['primary_NRO'];assert sha(Path(primary['path']))==primary['sha256']
 font=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json');assert sha(ROOT/'Switch-E2E5-TEST.nro')==font['root_cumulative_NRO_verified_against_documented_hash']
 git='C:/Program Files/Git/cmd/git.exe';pins={}
 for name,pin in previous['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
  assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 archive=Path('D:/logs2.7z');external={'path':str(archive),'status':'NOT VERIFIED; EXTERNAL FILE ABSENT'}
 if archive.is_file():
  external.update(status='PASS; CURRENT SHA256 MATCHES',sha256=sha(archive));assert external['sha256']=='0b6cd8d561c88ddddd58ea16e117a592f91c28b9a2c2146525f554b8453fd994'
 evidence={'status':'PARTIAL; ORIGINAL HANGAR/TCC/GALAXY CONSTRUCTOR/DESTRUCTOR PASS; CAMPAIGN/PLAYER NOT GENERATED',
  'runtime':runtimes,'SDK':sdk,'external_logs2_archive':external,'clean_upstream_pins':pins,
  'protected_game_sha256':previous['protected_game_sha256'],'protected_frozen_sha256':previous['protected_frozen_sha256'],
  'preserved_E590_packages':previous['preserved_E590_packages'],'primary_NRO':primary,'audit_sha256':sha(Path(__file__).resolve()),
  'new_NRO':False,'commit':False,'push':False,'remaining':['original gameplay definitions and ship/script template initialization','original full new-campaign generation with real Player/Planet','original Gov OnOpen/result/reward/history/return','natural victory','native/deko FRun runtime and user hardware acceptance']}
 with output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status']);print(sdk['artifacts'])
if __name__=='__main__':main()
