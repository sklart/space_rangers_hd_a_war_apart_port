"""Original order methods, real HAL world and partial MSVC ASan."""
import argparse
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--baseline',choices=('remove','rotate'));args=parser.parse_args()
 base=ROOT/'build/matrixgame/e2e583-robot-orders';check.BASE=base/('baseline-'+args.baseline if args.baseline else 'fixed');check.BASE.mkdir(parents=True,exist_ok=True)
 check.NATIVE=ROOT/'build/matrixgame/e2e582-ai-ignore/native-build' if args.baseline else base/'native-build'
 text=(ROOT/'tests/probe_matrixgame_hosted_windows_map.cpp').read_text();old='   runtime.InitializeOriginalGame(options);assert(runtime.OriginalGameReady() && g_MatrixMap);';assert text.count(old)==1
 text='void VerifyRobotOrders();\n'+text.replace(old,old+'\n   VerifyRobotOrders();',1);probe=check.BASE/'orders_probe.cpp'
 if probe.exists():assert probe.read_text()==text
 else:probe.write_text(text,encoding='utf-8')
 actor=check.NATIVE/'native/source/MatrixGame/src/MatrixRobot.cpp'
 if args.baseline:
  frozen=check.BASE/'baseline-robot.cpp'
  if not frozen.exists():frozen.write_bytes(actor.read_bytes())
  actor=frozen
 check.PROBE=probe;check.EXTRA_SOURCES=[ROOT/'tests/matrixgame_robot_order_queue.cpp',actor]
 check.EXTRA_FLAGS=['/fsanitize=address','/D_DISABLE_VECTOR_ANNOTATION','/D_DISABLE_STRING_ANNOTATION']
 if args.baseline=='rotate':check.EXTRA_FLAGS+=['/DORDER_TEST_ROTATE_ONLY']
 check.main()
