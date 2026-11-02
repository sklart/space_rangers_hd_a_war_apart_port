"""Separate SDK diagnostic with original PNG native F9 screenshots."""
import matrixgame_unmanaged_bitmap_sdk as sdk
if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e567-screenshot/sdk'
    sdk.main()
