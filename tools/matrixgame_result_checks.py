"""Original menu surrender -> statistics -> SaveResult -> owning host cleanup."""
import sys
import matrixgame_hosted_windows_map_checks as check
import matrixgame_diagnostic_session_checks as policy
import matrixgame_interactive_diagnostic_sdk as sdk
from matrixgame_standard_nro_audit import ROOT,read
BASE=ROOT/'build/matrixgame/e2e589-original-result'

def exclusive(path,text):
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')

def windows():
 check.BASE=BASE/'windows';check.BASE.mkdir(parents=True,exist_ok=True);check.NATIVE=ROOT/'build/matrixgame/e2e583-robot-orders/native-build'
 text=(ROOT/'tests/probe_matrixgame_hosted_windows_loop.cpp').read_text(encoding='utf-8')
 rejection='''
void RejectResult(NativeInitRuntime& runtime){
 SRobotGameState output{11,22,33,44,55,66};bool rejected=false;
 try{runtime.SaveOriginalGameResult(output);}catch(const std::logic_error&){rejected=true;}
 assert(rejected && output.m_Time==11 && output.m_BuildRobot==22 && output.m_KillRobot==33 && output.m_BuildTurret==44 && output.m_KillTurret==55 && output.m_KillBuilding==66);
}
'''
 replacements={
  '#include "init_runtime.hpp"':'#include "init_runtime.hpp"\n#include "MatrixSide.hpp"\n#include "engine_callbacks.hpp"',
  'std::wstring matrix_root,output_root;':'std::wstring matrix_root,output_root;'+rejection,
  '  case 209:g_Flags|=GFLAG_EXITLOOP;break;':'''  case 206:Key(27);assert(g_MatrixMap->IsPaused());std::puts("RESULT_INPUT menu_ESC=PASS");break;
  case 207:Key(83);assert(g_MatrixMap->m_DialogModeHints.size()>1);std::puts("RESULT_INPUT surrender_S_confirmation=PASS");break;
  case 208:Key(13);assert(g_ExitState==4);std::puts("RESULT_INPUT confirmed_surrender_original_state4=PASS");break;
  case 209:Key(13);assert(g_ExitState==4 && (g_Flags&GFLAG_EXITLOOP));std::puts("RESULT_INPUT statistics_Enter_original_exit=PASS");break;''',
  'output_paths::NativeScope private_outputs(output_root.c_str());NativeInitRuntime runtime;':'output_paths::NativeScope private_outputs(output_root.c_str());NativeInitRuntime runtime;RejectResult(runtime);',
  'native_loop::Scope scope(backend);g_Flags&=~GFLAG_EXITLOOP;':'native_loop::Scope scope(backend);g_ExitState=0;g_Flags&=~GFLAG_EXITLOOP;',
  'backend.draws==208 && g_MatrixMap->GetTime()-backend.start_time==5125':'backend.draws==208 && g_MatrixMap->GetTime()-backend.start_time==5050 && g_ExitState==4',
  'HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125':'HOSTED_ORIGINAL_LOOP draws=208 elapsed=5050',
  '   runtime.Deinit();assert(runtime.LastStateRestoreResult()':'   SRobotGameState saved{};runtime.SaveOriginalGameResult(saved);\n   const auto* side=g_MatrixMap->GetPlayerSide();assert(saved.m_Time==side->GetStatValue(STAT_TIME) && saved.m_BuildRobot==side->GetStatValue(STAT_ROBOT_BUILD) && saved.m_KillRobot==side->GetStatValue(STAT_ROBOT_KILL) && saved.m_BuildTurret==side->GetStatValue(STAT_TURRET_BUILD) && saved.m_KillTurret==side->GetStatValue(STAT_TURRET_KILL) && saved.m_KillBuilding==side->GetStatValue(STAT_BUILDING_KILL));\n   const auto returned=ReadEngineStatistics(saved);\n   runtime.Deinit();RejectResult(runtime);assert(returned.signed_time_ms==saved.m_Time && returned.robots_built==saved.m_BuildRobot && returned.robots_destroyed==saved.m_KillRobot && returned.turrets_built==saved.m_BuildTurret && returned.turrets_destroyed==saved.m_KillTurret && returned.buildings_destroyed==saved.m_KillBuilding);\n   std::printf("ORIGINAL_RESULT state=%d time=%d built_robots=%d killed_robots=%d built_turrets=%d killed_turrets=%d killed_buildings=%d six_original_stats=PASS pre_post_reject_unchanged=PASS owning_host_live=PASS\\n",g_ExitState,returned.signed_time_ms,returned.robots_built,returned.robots_destroyed,returned.turrets_built,returned.turrets_destroyed,returned.buildings_destroyed);\n   assert(runtime.LastStateRestoreResult()',
  'E581-Windows-HAL-hosted-loop':'E589-Windows-HAL-original-result',
 }
 for before,after in replacements.items():assert text.count(before)==1;text=text.replace(before,after,1)
 probe=check.BASE/'result_loop.cpp';exclusive(probe,text);check.PROBE=probe
 check.EXTRA_SOURCES=[ROOT/'port/matrixgame/native-engine'/name for name in ('original_game.cpp','native_form_session.cpp','native_form_backend.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp')]
 check.EXTRA_SOURCES+=[check.NATIVE/'native/source/MatrixLib/3G/OriginalNativeLoop.cpp',check.NATIVE/'native/source/MatrixGame/src/OriginalNativeForm.cpp'];check.main()
 record=read(max(check.BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));assert record['status'].startswith('PASS') and record['stdout'].count('six_original_stats=PASS pre_post_reject_unchanged=PASS owning_host_live=PASS')==2

if __name__=='__main__':
 kind=sys.argv[1]
 if kind=='windows':windows()
 elif kind=='sdk':sdk.BASE=BASE/'sdk';sdk.main()
 elif kind in ('policy-windows','policy-host','policy-arm64'):
  policy.BASE=BASE/'policy';sys.argv=[sys.argv[0],kind.removeprefix('policy-')];policy.main()
 else:raise ValueError(kind)
