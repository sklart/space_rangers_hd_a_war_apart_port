"""Addressed scalar/lease regression for the light-slot capture contract."""
from pathlib import Path
import matrixgame_stored_state_checks as checks
if __name__=='__main__':
 checks.BASE=checks.ROOT/'build/matrixgame/e2e533-form/state'
 checks.SOURCE=checks.ROOT/'build/matrixgame/e2e533-form/sdk/source'
 checks.TESTS=['tests/test_matrixgame_stored_state.cpp',str((checks.SOURCE/'MatrixLib/3G/DeviceState.cpp').relative_to(checks.ROOT))]
 checks.main()
