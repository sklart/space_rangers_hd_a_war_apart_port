"""Separate diagnostic SDK package with scoped Rangers callback integration."""
import matrixgame_unmanaged_bitmap_sdk as sdk
if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e568-native-services/sdk'
    sdk.main()
