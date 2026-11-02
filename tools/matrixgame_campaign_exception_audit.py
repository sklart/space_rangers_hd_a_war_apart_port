"""Audit real native-exception recovery separately from natural battle completion."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_legacy_wav_sdk_dependency import members

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
 base=ROOT/'build/matrixgame/e2e603-campaign-exception';paths={'Windows':base/'windows/1/result.json','SDK':base/'sdk/1/result.json'};gates={name:read(path) for name,path in paths.items()}
 for name,gate in gates.items():
  assert gate['status'].startswith('PASS') and all(c['exit']==0 for c in gate['commands']),name;check_hashes(gate['sources_sha256'])
 windows=gates['Windows'];check_hashes(windows['objects_sha256']);check_hashes(windows['reused_inputs_sha256']);assert sha(paths['Windows'].parent/'campaign-exception.exe')==windows['binary_sha256']
 log=(paths['Windows'].parent/'runtime.log').read_text(encoding='utf-8');assert log==windows['stdout']
 for marker in ('original_Init_Draw_then_typed_throw=PASS','original_exception_type=PASS real_snapshot_restore=PASS no_reward_or_history=PASS pending_request_cleared=PASS','original_campaign_UI_script_host_cleanup=PASS','configured_before_battle_equals_after_FRun=PASS real_SDK_readback=PASS'):assert marker in log,marker
 assert 'COMPLETION_NATURAL_DIALOG' not in log
 journals=list((paths['Windows'].parent/'runtime/output/journals').glob('*/run.jsonl'));assert len(journals)==1;events=[json.loads(line) for line in journals[0].read_text(encoding='utf-8').splitlines()]
 assert [e['seq'] for e in events]==list(range(1,len(events)+1));assert events[-1]['event']=='journal_close' and any(e['event']=='ok' and e['operation']=='B.CGame::Init' for e in events);assert not any(e['event'] in ('error','failure') for e in events)
 sdk=gates['SDK'];check_hashes(sdk['dependency_objects_sha256']);assert sdk['changed_members']==['Robot.o','fGov.o'] and sdk['production_generated']==['Robot.cpp','fGov.cpp'];assert members(ROOT/sdk['archive_path'])==sdk['archive_members_sha256']
 for name,info in sdk['artifacts'].items():assert sha(ROOT/name)==info['sha256']
 symbols=subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-g',str(paths['SDK'].parent/'matrixgame-campaign-exception.elf')],text=True);assert not any(' U ' in line for line in symbols.splitlines())
 prior=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(prior[key])
 assert sha(Path(prior['primary_NRO']['path']))==prior['primary_NRO']['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==prior['root_NRO_sha256']
 for info in prior['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in prior['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 result={'status':'PASS WINDOWS REAL INIT/DRAW/C++ EXCEPTION/CALLER AND SNAPSHOT RESTORE/CLEANUP; SDK PRODUCTION COMPILE/LINK','gates':{k:{'path':str(p.relative_to(ROOT)),'sha256':sha(p),'status':gates[k]['status']} for k,p in paths.items()},'journal_sha256':sha(journals[0]),'natural_victory':'NOT PROVEN','natural_outcome_in_this_test':'NOT CLAIMED','instrumentation':'PARTIAL Windows MSVC ASAN; original dependency corpus not fully instrumented','SDK_runtime':'NOT RUN','native_deko_FRun':'NOT RUN','Switch':'NOT RUN','new_NRO':False,'protected_inputs_and_previous_NRO':'HASHES VERIFIED AGAINST E601 MANIFEST','clean_upstream_pins':prior['clean_upstream_pins'],'commit':False,'push':False,'audit_sha256':sha(Path(__file__).resolve())}
 with args.output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
 print(result['status'])

if __name__=='__main__':main()
