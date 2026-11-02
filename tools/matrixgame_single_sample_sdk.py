"""Separate SDK NRO for the single-sample state correction."""
from pathlib import Path
import matrixgame_unmanaged_bitmap_sdk as sdk

if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e562-single-sample/sdk'
    sdk.main()
