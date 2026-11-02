"""Separate diagnostic NRO with the actual native wireframe draw path."""
import matrixgame_unmanaged_bitmap_sdk as sdk

if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e563-wireframe/sdk'
    sdk.main()
