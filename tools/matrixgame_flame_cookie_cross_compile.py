"""Compile the complete corrected flame TU with host sanitizers and ARM64."""
import matrixgame_ai_ignore_cross_compile as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e590-completed-battle'
 check.ENGINE=check.BASE/'fixed/generated'
 check.SOURCES=[check.ENGINE/'MatrixGame/src/Effects/MatrixEffectFlame.cpp']
 check.main()
