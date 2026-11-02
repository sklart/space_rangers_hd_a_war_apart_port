"""Reuse the verified CPU dependency/link runner for the original script host lifecycle."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 assert len(sys.argv)==2 and sys.argv[1] in ('asan','arm64')
 text=(ROOT/'tools/matrixgame_government_cpu_checks.py').read_text(encoding='utf-8')
 changes={
  'e2e594-government-session':'e2e595-script-session',
  'rangers_government_session.cpp':'rangers_script_session.cpp',
  'probe_matrixgame_government_layout.cpp':'probe_matrixgame_script_session.cpp',
  'rangers_government_session.hpp':'rangers_script_session.hpp',
  "assert result['stdout'].count('original_Gov_constructor_config_layout=PASS real_callbacks=PASS duplicate_owner=REJECTED OnOpen_Galaxy=NOT_RUN')==2":"assert result['stdout'].count('original_VM_abs_assignment=42 path_allocate_recycle=PASS auto_manual_events=PASS real_worker_standby=PASS Galaxy=NOT_CREATED')==2",
  "assert 'original_partial_tree_failure_rollback=PASS' in result['stdout'] and 'borrowed_root_restore=PASS foreign_roots=REJECTED no_save_load=PASS CPU_LAYOUT_ONLY' in result['stdout']":"assert 'original_Finalize=PASS events_worker_closed=PASS pool_heads_counts_callback_restored=PASS foreign_owner=REJECTED no_save_load=PASS NO_WORLD_GENERATION' in result['stdout']",
  'ORIGINAL GOVERNMENT UI LAYOUT/ROLLBACK/CLEANUP EXECUTED; NO ONOPEN/GALAXY':'ORIGINAL SCRIPT VM/EVENTS/WORKER/PATH LIFECYCLE EXECUTED; NO GALAXY',
 }
 for before,after in changes.items():assert text.count(before)==1,before;text=text.replace(before,after,1)
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 out=ROOT/'build/matrixgame/e2e595-script-session/fixture';out.mkdir(parents=True,exist_ok=True);path=out/'cpu.py'
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,path,sys.argv[1]],check=True)
