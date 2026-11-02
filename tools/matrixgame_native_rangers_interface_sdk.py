"""Standard SDK complete Robot interface library and retained real link closure."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 source=ROOT/'tools/matrixgame_native_rangers_run_sdk.py';text=source.read_text(encoding='utf-8')
 replacements={
  "BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run/sdk'":"BASE=ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk'",
  "'matrixgame-native-rangers-run','matrixgame-native-interactive'":"'matrixgame-native-rangers-interface','matrixgame-native-interactive'",
  "archive=BASE/'libmatrixgame-native-rangers-run.a'":"archive=BASE/'libmatrixgame-native-rangers-interface.a'",
  "'NativeRangersRunBinding' in line.split()[-1]":"'NativeRangersInterface' in line.split()[-1]",
  'matrixgame-native-rangers-run-link.elf':'matrixgame-native-rangers-interface-link.elf',
  'rangers-run-link.map':'rangers-interface-link.map',
  "args+=['-Wl,--whole-archive',str(archive),'-Wl,--no-whole-archive'":"args+=['-Wl,--whole-archive',str(archive),str(BASE/'libmatrixgame-native-rangers-run.a'),'-Wl,--no-whole-archive'",
  'NATIVE RANGERS RUN LIBRARY':'COMPLETE NATIVE ROBOT INTERFACE LIBRARY',
 }
 for before,after in replacements.items():
  count=2 if before=='rangers-run-link.map' else 1
  assert text.count(before)==count,before
  text=text.replace(before,after,count)
 text="import sys\nsys.path.insert(0,"+repr(str(ROOT/'tools'))+")\n"+text
 out=ROOT/'build/matrixgame/e2e592-native-rangers-interface/sdk-fixture';out.mkdir(parents=True,exist_ok=True);script=out/'run.py'
 if script.exists():assert script.read_text(encoding='utf-8')==text
 else:script.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,script],check=True)
