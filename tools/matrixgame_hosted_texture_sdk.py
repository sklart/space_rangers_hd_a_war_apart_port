"""Separate standard SDK NRO with original cache classes and pointer-safe queues."""
import matrixgame_hosted_diagnostic_sdk as build
from matrixgame_standard_nro_audit import ROOT,sha
if __name__=='__main__':
 previous=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
 before=sha(previous)
 build.BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map/sdk'
 build.main()
 assert sha(previous)==before=='9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356'
