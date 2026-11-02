"""Separate normal SDK diagnostic NRO with the original AI ignore FIFO fix."""
import matrixgame_hosted_diagnostic_sdk as build
from matrixgame_standard_nro_audit import ROOT,sha
if __name__=='__main__':
 previous=ROOT/'build/matrixgame/e2e580-loading-capture/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
 assert sha(previous)=='b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b'
 build.BASE=ROOT/'build/matrixgame/e2e582-ai-ignore/sdk'
 build.main()
 assert sha(previous)=='b6ec6ce859ce21c9471084d1e63be8a45d7f492f0f98a31e0b3cb421e4ee6c8b'
