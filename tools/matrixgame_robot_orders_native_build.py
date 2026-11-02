"""Fresh standard production archives with original robot order fixes."""
import matrixgame_hosted_windows_native_build as build
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 build.BASE=ROOT/'build/matrixgame/e2e583-robot-orders/native-build'
 build.main()
