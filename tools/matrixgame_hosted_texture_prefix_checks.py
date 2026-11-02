"""Recheck real early Init rollback with the corrected original texture factory."""
import matrixgame_hosted_windows_native_checks as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map/native-build'
 check.main()
