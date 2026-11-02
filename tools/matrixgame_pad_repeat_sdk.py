"""Separate diagnostic NRO with keyboard-only typematic dispatch."""
import matrixgame_unmanaged_bitmap_sdk as sdk

if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e564-pad-repeat/sdk'
    sdk.main()
