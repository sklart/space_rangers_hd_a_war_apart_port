"""Regression for the shared real CBitmap/CFile screenshot conversion."""
import matrixgame_screenshot_checks as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e580-loading-capture/screenshot'
 check.ENGINE=ROOT/'build/matrixgame/e2e580-loading-capture/native-build/native/source'
 check.main()
