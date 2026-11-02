"""Standard SDK target and real retained Galaxy prerequisite API closure."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 text=(ROOT/'tools/matrixgame_government_sdk.py').read_text(encoding='utf-8')
 for before,after in (
  ('e2e594-government-session','e2e596-galaxy-prerequisites'),
  ('matrixgame-rangers-government','matrixgame-rangers-galaxy-prerequisites'),
  ('RangersGovernmentSession','RangersGalaxyPrerequisites'),
  ('government','galaxy-prerequisites'),
  ('GOVERNMENT','GALAXY PREREQUISITES'),
 ):
  assert before in text,before;text=text.replace(before,after)
 before="['-Wl,--whole-archive',str(archive),'-Wl,--no-whole-archive']"
 assert text.count(before)==1
 text=text.replace(before,"['-Wl,--whole-archive',str(archive),str(archive.parent/'libmatrixgame-rangers-script.a'),'-Wl,--no-whole-archive']")
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 out=ROOT/'build/matrixgame/e2e596-galaxy-prerequisites/fixture';out.mkdir(parents=True,exist_ok=True);path=out/'sdk.py'
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,path],check=True)
