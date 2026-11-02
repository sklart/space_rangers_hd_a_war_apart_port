"""Fresh standard Windows production archives for the AI ignore correction."""
import matrixgame_hosted_windows_native_build as build
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 build.BASE=ROOT/'build/matrixgame/e2e582-ai-ignore/native-build'
 build.main()
