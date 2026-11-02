"""Build the standard script owner SDK target and retain its real API closure."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 text=(ROOT/'tools/matrixgame_government_sdk.py').read_text(encoding='utf-8')
 for before,after in (
  ('e2e594-government-session','e2e595-script-session'),
  ('matrixgame-rangers-government','matrixgame-rangers-script'),
  ('RangersGovernmentSession','RangersScriptSession'),
  ('government','script'),
  ('GOVERNMENT','SCRIPT'),
  ('assert len(roots)>=5','assert len(roots)==4'),
 ):
  assert before in text,before
  text=text.replace(before,after)
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 out=ROOT/'build/matrixgame/e2e595-script-session/fixture';out.mkdir(parents=True,exist_ok=True);path=out/'sdk-owner-api.py'
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,path],check=True)
