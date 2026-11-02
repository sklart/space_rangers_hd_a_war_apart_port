"""Separate diagnostic NRO with copied native settings and minification AF."""
import matrixgame_unmanaged_bitmap_sdk as sdk

if __name__=='__main__':
    sdk.BASE=sdk.ROOT/'build/matrixgame/e2e565-native-settings/sdk'
    sdk.main()
