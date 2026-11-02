"""Original player move command/pathfinding inside the real hosted form loop."""
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT,read
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e584-hosted-movement';check.BASE.mkdir(parents=True,exist_ok=True)
 check.NATIVE=ROOT/'build/matrixgame/e2e583-robot-orders/native-build'
 text=(ROOT/'tests/probe_matrixgame_hosted_windows_loop.cpp').read_text()
 replacements={
  '#include "init_runtime.hpp"':'#include "init_runtime.hpp"\n#include "matrixgame_movement_probe.hpp"',
  ' native_input::EngineSink input;platform3g::AppletEngineEvents events;':' native_input::EngineSink input;platform3g::AppletEngineEvents events;MovementProbe movement;',
  '  case 209:g_Flags|=GFLAG_EXITLOOP;break;':'  case 9:movement.Issue();break;\n  case 209:movement.Verify();g_Flags|=GFLAG_EXITLOOP;break;',
  'E581-Windows-HAL-hosted-loop':'E584-Windows-HAL-hosted-movement',
 }
 for before,after in replacements.items():assert text.count(before)==1;text=text.replace(before,after,1)
 probe=check.BASE/'movement_loop.cpp'
 if probe.exists():assert probe.read_text()==text
 else:probe.write_text(text,encoding='utf-8')
 check.PROBE=probe;check.EXTRA_SOURCES=[ROOT/'tests/matrixgame_movement_probe.cpp']
 check.EXTRA_SOURCES+=[ROOT/'port/matrixgame/native-engine'/name for name in ('native_form_session.cpp','native_form_backend.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp')]
 check.EXTRA_SOURCES+=[check.NATIVE/'native/source/MatrixLib/3G/OriginalNativeLoop.cpp',check.NATIVE/'native/source/MatrixGame/src/OriginalNativeForm.cpp']
 check.EXTRA_FLAGS=['/I'+str(ROOT/'tests')];check.main()
 records=sorted(check.BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);record=read(records[0]);assert record['status'].startswith('PASS')
 assert record['stdout'].count('MOVEMENT original_player_command_navigation_simulation=PASS no_position_injection=PASS live_robot=PASS')==2
