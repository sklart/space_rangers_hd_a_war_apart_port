"""Compile changed complete robot TU/oracle on host-ASan and ARM64; no runtime."""
import matrixgame_ai_ignore_cross_compile as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e583-robot-orders'
 check.ENGINE=check.BASE/'native-build/native/source'
 check.SOURCES=[check.ENGINE/'MatrixGame/src/MatrixRobot.cpp',ROOT/'tests/matrixgame_robot_order_queue.cpp']
 check.main()
