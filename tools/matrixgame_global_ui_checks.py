"""Exercise the full original global initializer with real E595 host dependencies."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 assert len(sys.argv)==2 and sys.argv[1] in ('windows-asan','asan','arm64')
 text=(ROOT/'tools/matrixgame_galaxy_prerequisites_checks.py').read_text(encoding='utf-8')
 changes={
  'e2e596-galaxy-prerequisites':'e2e597-global-ui',
  'rangers_galaxy_prerequisites.cpp':'rangers_global_ui_session.cpp',
  'rangers_galaxy_prerequisites.hpp':'rangers_global_ui_session.hpp',
  'probe_matrixgame_galaxy_prerequisites.cpp':'probe_matrixgame_global_ui.cpp',
  "assert result['stdout'].count('original_Hangar_config_layout=PASS original_TCCInterface=PASS original_TGalaxy_create_destroy=PASS integrity_operations=UNMODIFIED campaign_Player=NOT_GENERATED')==2":"assert result['stdout'].count('original_full_initializer=PASS duplicate_owner=REJECTED campaign_generation=NOT_RUN')==2",
  "assert 'partial_layout_rollback=PASS foreign_TCC_roots_duplicate=REJECTED original_cleanup_borrowed_roots=PASS no_save_load=PASS campaign_generation=NOT_RUN' in result['stdout']":"assert result['stdout'].count('original_finalize_reset=PASS omitted_owners_released=PASS no_save_load=PASS')==2",
  'ORIGINAL HANGAR/TCC/GALAXY LIFECYCLE; NO GENERATED CAMPAIGN':'ORIGINAL FULL GLOBAL UI/GAMEPLAY INITIALIZER; NO GENERATED CAMPAIGN',
 }
 for before,after in changes.items():assert text.count(before)==1,before;text=text.replace(before,after,1)
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 out=ROOT/'build/matrixgame/e2e597-global-ui/fixture';out.mkdir(parents=True,exist_ok=True);path=out/'checks.py'
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,path,sys.argv[1]],check=True)
