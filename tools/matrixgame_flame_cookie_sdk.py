"""New standard diagnostic packages with corrected flame cookie and legacy WAV."""
import matrixgame_interactive_diagnostic_sdk as build
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 build.build.HOST_DEPENDENCY_RESULT=ROOT/'build/matrixgame/e2e590-completed-battle/sdk-dependency/result.json'
 build.build.HOST_ARCHIVE=ROOT/'build/matrixgame/e2e590-completed-battle/sdk-dependency/liboriginal-rangers-host-wave-compat.a'
 build.BASE=ROOT/'build/matrixgame/e2e590-completed-battle/sdk';build.main()
