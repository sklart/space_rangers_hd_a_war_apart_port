"""Audit original full global initialization while keeping campaign/Player unproven."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 text=(ROOT/'tools/matrixgame_galaxy_prerequisites_audit.py').read_text(encoding='utf-8')
 changes={
  'e2e596-galaxy-prerequisites':'e2e597-global-ui',
  "assert r['stdout'].count('controls=341 original_Hangar_config_layout=PASS original_TCCInterface=PASS original_TGalaxy_create_destroy=PASS integrity_operations=UNMODIFIED campaign_Player=NOT_GENERATED')==2":"assert r['stdout'].count('screens=36 controls=10826 script_templates=40 global_variables=32 planet_templates=207 stars=73 original_full_initializer=PASS duplicate_owner=REJECTED campaign_generation=NOT_RUN')==2",
  "assert 'partial_layout_rollback=PASS foreign_TCC_roots_duplicate=REJECTED original_cleanup_borrowed_roots=PASS no_save_load=PASS campaign_generation=NOT_RUN' in r['stdout']":"assert 'original_partial_failure_rollback=PASS unregistered_foreign_screen=REJECTED' in r['stdout'] and r['stdout'].count('original_finalize_reset=PASS omitted_owners_released=PASS no_save_load=PASS')==2",
  "len(sdk['retained_API_symbols'])>=5":"len(sdk['retained_API_symbols'])==4",
  'reports/matrixgame-e2e595-script-session-evidence.json':'reports/matrixgame-e2e596-galaxy-prerequisites-evidence.json',
  'PARTIAL; ORIGINAL HANGAR/TCC/GALAXY CONSTRUCTOR/DESTRUCTOR PASS; CAMPAIGN/PLAYER NOT GENERATED':'PARTIAL; FULL ORIGINAL GLOBAL UI/GAMEPLAY INITIALIZATION PASS; CAMPAIGN/PLAYER NOT GENERATED',
  "'original gameplay definitions and ship/script template initialization',":"'LP64 StorageEntry allocation in original campaign generator',",
 }
 for before,after in changes.items():assert text.count(before)==1,before;text=text.replace(before,after,1)
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 out=ROOT/'build/matrixgame/e2e597-global-ui/fixture';out.mkdir(parents=True,exist_ok=True);path=out/'audit.py'
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,path,*sys.argv[1:]],check=True)
