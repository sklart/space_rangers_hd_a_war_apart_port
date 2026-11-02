"""Fresh current native build; preserve all E579 outputs and evidence."""
import matrixgame_hosted_windows_native_build as build
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 build.BASE=ROOT/'build/matrixgame/e2e580-loading-capture/native-build'
 build.main()
