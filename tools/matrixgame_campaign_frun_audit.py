"""Audit actual integrated Gov defeats; victory is accepted only from a real completed run."""
import argparse,json,re,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes

def verify(path):
 gate=read(path);assert gate['status'].startswith('PASS') and all(c['exit']==0 for c in gate['commands'])
 check_hashes(gate['reused_inputs_sha256']);check_hashes(gate['objects_sha256']);assert sha(path.parent/'campaign-frun.exe')==gate['binary_sha256']
 compiled={k:v for k,v in gate['sources_sha256'].items() if (ROOT/k).parent==path.parent};assert len(compiled)==(5 if gate.get('native_exception_restore') else 3);check_hashes(compiled)
 # The complete expanded compiler inputs are immutable. Shared fixture/helper
 # sources may have gained another test mode since this historical run.
 log=(path.parent/'runtime.log').read_text(encoding='utf-8');assert log==gate['stdout']
 for marker in ('original_Gov_history=PASS actual_statistics=PASS','restored_player_planet=PASS','requested_Gov_return=PASS','original_campaign_UI_script_host_cleanup=PASS','configured_before_battle_equals_after_FRun=PASS real_SDK_readback=PASS'):
  assert marker in log,marker
 outcome=re.search(r'GOV_FRUN actual_outcome=([23]) ',log);assert outcome
 assert f'COMPLETION_NATURAL_DIALOG state={outcome[1]} ' in log
 journals=list((path.parent/'runtime/output/journals').glob('*/run.jsonl'));assert len(journals)==1;events=[json.loads(line) for line in journals[0].read_text(encoding='utf-8').splitlines()]
 assert [e['seq'] for e in events]==list(range(1,len(events)+1)) and events[-1]['event']=='journal_close' and any(e['event']=='ok' and e['operation']=='B.CGame::Init' for e in events)
 assert not any(e['event'] in ('error','failure') for e in events)
 user=path.parent/'runtime/output/rangers-user';files=list(user.rglob('*'));assert not any(p.is_file() and (p.suffix.lower() in ('.sav','.save','.sn') or p.name.lower()=='save.tmp') for p in files)
 return {'result':gate,'result_path':str(path.relative_to(ROOT)),'result_sha256':sha(path),'actual_outcome':int(outcome[1]),'compiled_sources_current_sha256':compiled,'journal_sha256':sha(journals[0]),'entry_boundary':'Gov OnOpen; original UI auto-save handler not called','Rangers_UI_presentation':'real RGB565 canvas/draw/hash; host provider does not prove physical Gov UI display','MatrixGame_presentation':'real Windows D3D9 HAL; original form Draw and SDK readback'}

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--victory-result',type=Path);parser.add_argument('--extra-result',type=Path,action='append',default=[]);args=parser.parse_args();base=ROOT/'build/matrixgame/e2e602-campaign-frun'
 defeats=[verify(base/'windows/4/result.json'),verify(base/'windows-difficulty3/1/result.json')];assert all(g['actual_outcome']==2 for g in defeats)
 victory=verify(args.victory_result.resolve()) if args.victory_result else None
 if victory:assert victory['actual_outcome']==3
 additional=[verify(path.resolve()) for path in args.extra_result]
 assert all(g['actual_outcome']==2 for g in additional),'pass a natural victory through --victory-result'
 prior=read(ROOT/'reports/matrixgame-e2e601-revolution-seed-evidence.json')
 for key in ('protected_game_sha256','protected_frozen_sha256'):check_hashes(prior[key])
 assert sha(Path(prior['primary_NRO']['path']))==prior['primary_NRO']['sha256'];assert sha(ROOT/'Switch-E2E5-TEST.nro')==prior['root_NRO_sha256']
 for info in prior['preserved_E590_packages'].values():assert sha(ROOT/info['path'])==info['sha256']
 git='C:/Program Files/Git/cmd/git.exe'
 for name,pin in prior['clean_upstream_pins'].items():
  folder=ROOT/'upstream'/name;assert subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
 evidence={'status':'PASS INTEGRATED WINDOWS GOV/FRUN/NATURAL '+('DEFEAT AND VICTORY' if victory else 'DEFEAT ONLY')+'/REAL HISTORY/REWARD/RETURN','defeats':defeats,'victory':victory or {'status':'NOT PROVEN'},'Windows_instrumentation':'PARTIAL MSVC ASAN; not every original dependency TU instrumented','Switch':'NOT RUN','native_deko_FRun':'NOT RUN','main_Galaxy_UI_loop':'NOT RUN; original OnOpen/OnClose/stack and RequestedScreenId transition checked','new_NRO':False,'commit':False,'push':False,'protected_game_sha256':prior['protected_game_sha256'],'protected_frozen_sha256':prior['protected_frozen_sha256'],'primary_NRO':prior['primary_NRO'],'root_NRO_sha256':prior['root_NRO_sha256'],'clean_upstream_pins':prior['clean_upstream_pins'],'audit_sha256':sha(Path(__file__).resolve())}
 evidence['additional_completed_runs']=additional
 with args.output.open('x',encoding='utf-8') as stream:json.dump(evidence,stream,indent=2);stream.write('\n')
 print(evidence['status'])

if __name__=='__main__':main()
