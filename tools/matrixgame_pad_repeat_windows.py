"""Actual original robot hold scenario on Windows HAL; distinct baseline/candidate."""
import argparse
import subprocess
import sys
from pathlib import Path
from matrixgame_pad_repeat_checks import ROOT,BASE


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('baseline','candidate'));args=parser.parse_args()
    source=ROOT/'build/matrixgame/e2e563-wireframe/sdk/source'
    command=[sys.executable,ROOT/'tools/matrixgame_actual_draw_checks.py','windows','--selection',
        '--selection-source',ROOT/'tests/matrixgame_windows_pad_hold.cpp','--output',BASE/('map-'+args.kind),
        '--resource-root',ROOT/'build/matrixgame/e2e519/skin-borrow-windows/runtime',
        '--robot-source',source/'MatrixGame/src/MatrixObjectRobot.cpp','--diagnostic-source',source]
    if args.kind=='baseline':command+=['--input-source',BASE/'baseline/source/native_input.cpp']
    result=subprocess.run(list(map(str,command)),cwd=ROOT)
    raise SystemExit(result.returncode)


if __name__=='__main__':main()
