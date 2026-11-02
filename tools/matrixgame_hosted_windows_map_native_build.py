"""Current native archives in a new E579 directory; preserve E578 binaries."""
import matrixgame_hosted_windows_native_build as build
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 build.BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map/native-build'
 build.main()
