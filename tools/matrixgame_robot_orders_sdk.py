"""Separate normal SDK diagnostic NRO with original robot order queue fixes."""
import matrixgame_hosted_diagnostic_sdk as build
from matrixgame_standard_nro_audit import ROOT,sha
if __name__=='__main__':
 previous=ROOT/'build/matrixgame/e2e582-ai-ignore/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
 assert sha(previous)=='2519b5122f556cf9be1cbd8b53126e1fd8a9e50d833c68c4d763862afa2e0d5e'
 build.BASE=ROOT/'build/matrixgame/e2e583-robot-orders/sdk'
 build.main()
 assert sha(previous)=='2519b5122f556cf9be1cbd8b53126e1fd8a9e50d833c68c4d763862afa2e0d5e'
