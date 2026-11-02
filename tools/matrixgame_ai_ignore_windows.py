"""Partial MSVC ASan execution of original CInfo with real owning host/map."""
import argparse
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
 base=ROOT/'build/matrixgame/e2e582-ai-ignore'
 check.BASE=base/('baseline' if args.baseline else 'fixed');check.BASE.mkdir(parents=True,exist_ok=True)
 check.NATIVE=ROOT/'build/matrixgame/e2e580-loading-capture/native-build' if args.baseline else base/'native-build'
 source=(ROOT/'tests/probe_matrixgame_hosted_windows_map.cpp').read_text()
 old='   runtime.InitializeOriginalGame(options);assert(runtime.OriginalGameReady() && g_MatrixMap);'
 assert source.count(old)==1;source=source.replace(old,old+'\n   VerifyAiIgnoreQueue();',1)
 source='void VerifyAiIgnoreQueue();\n'+source
 probe=check.BASE/'ignore_probe.cpp'
 if probe.exists():assert probe.read_text()==source
 else:probe.write_text(source,encoding='utf-8')
 # Match the existing unannotated STL archive ABI. Address instrumentation
 # and memcpy overlap interception remain enabled in all new TUs.
 check.PROBE=probe;check.EXTRA_FLAGS=['/fsanitize=address','/D_DISABLE_VECTOR_ANNOTATION','/D_DISABLE_STRING_ANNOTATION']
 environment=base/'baseline-environment.cpp' if args.baseline else base/'native-build/native/source/MatrixGame/src/Logic/MatrixEnvironment.cpp'
 if args.baseline and not environment.exists():environment.write_bytes((check.NATIVE/'native/source/MatrixGame/src/Logic/MatrixEnvironment.cpp').read_bytes())
 check.EXTRA_SOURCES=[ROOT/'tests/matrixgame_ai_ignore_queue.cpp',environment]
 check.main()
