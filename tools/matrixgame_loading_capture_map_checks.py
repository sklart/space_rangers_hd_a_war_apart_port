"""Targeted Windows HAL regression for the refactored loading capture path."""
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e580-loading-capture/reference-map'
 check.NATIVE=ROOT/'build/matrixgame/e2e580-loading-capture/native-build'
 check.main()
