"""Audit production UiContext in a real campaign/battle/result-return run."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_campaign_frun_audit import verify

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--result',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
 path=args.result.resolve();verified=verify(path);gate=verified['result']
 assert gate['production_ui_context'] and gate['caller_lease'];check_hashes(gate['sources_sha256'])
 log=gate['stdout']
 for marker in ('production_UI_context=ACTIVE original_CFG_TCC_canvas_palette_cursors=PASS','CALLER_LEASE natural_return_window_owned=PASS battle_lease_released=PASS'):
  assert marker in log,marker
 snapshot=read(path.parent/'source-snapshot.json');assert snapshot['result_sha256']==sha(path) and set(snapshot['files'])==set(gate['sources_sha256'])
 for name,info in snapshot['files'].items():assert info['sha256']==gate['sources_sha256'][name] and sha(ROOT/info['snapshot'])==info['sha256']
 # Inventory the whole private user root; there must be no file of any kind.
 user=path.parent/'runtime/output/rangers-user';assert not any(p.is_file() for p in user.rglob('*'))
 protected=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(protected[key])
 assert sha(Path(protected['primary_NRO']['path']))==protected['primary_NRO']['sha256'] and sha(ROOT/'Switch-E2E5-TEST.nro')==protected['root_NRO_sha256']
 for info in protected['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 old=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/result.json');check_hashes(old['all_link_inputs_sha256']);check_hashes(old['protected_previous_NRO_sha256'])
 for name,info in old['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 package=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/package-result.json');assert sha(ROOT/package['zip'])==package['zip_sha256'];check_hashes(package['files_sha256'],ROOT/package['package'])
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in protected['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PASS WINDOWS PRODUCTION UI CONTEXT/ORIGINAL CAMPAIGN/FRUN/NATURAL OUTCOME/GOV RESULT RETURN/CLEANUP','run':verified,'source_snapshot_sha256':sha(path.parent/'source-snapshot.json'),'private_user_files':[],'protected_inputs_and_upstream_pins':'VERIFIED','E604_package_preserved_sha256':package['zip_sha256'],'Windows_instrumentation':'PARTIAL MSVC ASAN','native_GPU_runtime':'NOT RUN','native_UI_input':'NOT INTEGRATED','full_native_Gov_entry':'NOT RUN','natural_victory':gate['natural_victory'],'new_NRO':False,'commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status'])

if __name__=='__main__':main()
