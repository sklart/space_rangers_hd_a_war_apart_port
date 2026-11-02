"""Separate SDK diagnostic with both frame readbacks cancelled on abort."""
import matrixgame_unmanaged_bitmap_sdk as sdk
if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e566-readback-abort/sdk'
    sdk.main()
