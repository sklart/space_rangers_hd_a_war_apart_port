"""Separate normal SDK diagnostic NRO with owned native loading snapshot."""
import matrixgame_hosted_diagnostic_sdk as build
from matrixgame_standard_nro_audit import ROOT,sha
if __name__=='__main__':
 previous=ROOT/'build/matrixgame/e2e579-hosted-windows-map/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
 before=sha(previous)
 assert before=='020b6d3debff52d5ae1efb757b2aee10b6e9886692d83a2d2d8abac2cabf0474'
 build.BASE=ROOT/'build/matrixgame/e2e580-loading-capture/sdk'
 build.main()
 assert sha(previous)==before
