"""Audit real Windows caller handoff and native SDK closure without GPU claims."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_campaign_frun_audit import verify
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();base=ROOT/'build/matrixgame/e2e606-caller-presentation'
 paths={'Windows_exception':base/'windows/2/result.json','Windows_natural':ROOT/'build/matrixgame/e2e602-campaign-frun/windows-difficulty3-economy-campaign0-winter1_1e-heavy-squad-caller-lease/1/result.json','SDK':base/'sdk/2/result.json'};gates={k:read(p) for k,p in paths.items()};records={}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS') and all(c['exit']==0 for c in gate['commands']),name
  snapshot=read(paths[name].parent/'source-snapshot.json');assert snapshot['result_sha256']==sha(paths[name]);assert set(snapshot['files'])==set(gate['sources_sha256'])
  for source,info in snapshot['files'].items():assert info['sha256']==gate['sources_sha256'][source] and sha(ROOT/info['snapshot'])==info['sha256']
  # Current implementation is exactly the tested version at audit time.
  check_hashes(gate['sources_sha256']);records[name]={'path':str(paths[name].relative_to(ROOT)),'sha256':sha(paths[name]),'source_snapshot_sha256':sha(paths[name].parent/'source-snapshot.json'),'status':gate['status']}
 for name in ('Windows_exception','Windows_natural'):
  gate=gates[name];check_hashes(gate['objects_sha256']);check_hashes(gate['reused_inputs_sha256']);log=(paths[name].parent/'runtime.log').read_text(encoding='utf-8');assert log==gate['stdout']
  binary=paths[name].parent/('campaign-exception.exe' if name=='Windows_exception' else 'campaign-frun.exe');assert sha(binary)==gate['binary_sha256']
  for marker in ('configured_before_battle_equals_after_FRun=PASS real_SDK_readback=PASS','original_campaign_UI_script_host_cleanup=PASS'):assert marker in log
  journals=list((paths[name].parent/'runtime/output/journals').glob('*/run.jsonl'));assert len(journals)==1;events=[json.loads(line) for line in journals[0].read_text(encoding='utf-8').splitlines()];assert events[-1]['event']=='journal_close' and [e['seq'] for e in events]==list(range(1,len(events)+1));assert any(e['event']=='ok' and e['operation']=='B.CGame::Init' for e in events)
  records[name]['journal_sha256']=sha(journals[0])
 exception=gates['Windows_exception']['stdout'];assert 'CALLER_LEASE real_window_handoff=PASS original_Init_Draw=PASS reacquired_window=PASS' in exception and 'original_exception_type=PASS real_snapshot_restore=PASS no_reward_or_history=PASS' in exception and 'COMPLETION_NATURAL_DIALOG' not in exception
 natural=verify(paths['Windows_natural']);assert natural['actual_outcome']==2 and gates['Windows_natural']['caller_lease'];assert 'CALLER_LEASE natural_return_window_owned=PASS battle_lease_released=PASS' in gates['Windows_natural']['stdout']
 sdk=gates['SDK'];check_hashes(sdk['dependency_objects_sha256']);assert sdk['changed_members']==['Robot.o'];assert members(ROOT/sdk['archive_path'])==sdk['archive_members_sha256'];assert members(paths['SDK'].parent/'libmatrixgame-native-rangers-interface.a')==sdk['native_interface_archive_members_sha256']
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 symbols=subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',str(paths['SDK'].parent/'matrixgame-caller-presentation.elf')],text=True);assert not any(' U ' in line for line in symbols.splitlines()) and 'RangersCallerPresentation' in symbols
 old=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/result.json');check_hashes(old['all_link_inputs_sha256']);check_hashes(old['protected_previous_NRO_sha256'])
 for name,info in old['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 package=read(ROOT/'build/matrixgame/e2e604-current-diagnostic/1/package-result.json');assert sha(ROOT/package['zip'])==package['zip_sha256'];check_hashes(package['files_sha256'],ROOT/package['package'])
 prior=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json');check_hashes(prior['protected_game_sha256']);check_hashes(prior['protected_frozen_sha256'])
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in prior['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS REAL WINDOWS CALLER WINDOW/DEVICE/GAMMA HANDOFF: EXCEPTION AND NATURAL DEFEAT/RESULT/RETURN; NATIVE SDK COMPILE/LINK','gates':records,'Windows_instrumentation':'PARTIAL MSVC ASAN','native_GPU_runtime':'NOT RUN','native_full_Gov_context':'NOT RUN','natural_victory':'NOT PROVEN','new_NRO':False,'E604_package_preserved_sha256':package['zip_sha256'],'protected_inputs_and_upstream_pins':'VERIFIED','commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status'])

if __name__=='__main__':main()
