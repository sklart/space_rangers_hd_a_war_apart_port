"""Recheck readback cancellation after adding the completed-frame selector."""
import shutil
import matrixgame_readback_abort_checks as checks
if __name__=='__main__':
    baseline=checks.BASE/'platform_switch-baseline.cpp'
    checks.BASE=checks.ROOT/'build/matrixgame/e2e567-screenshot/readback-regression'
    checks.BASE.mkdir(parents=True,exist_ok=True)
    target=checks.BASE/baseline.name
    if not target.exists():shutil.copyfile(baseline,target)
    checks.main()
