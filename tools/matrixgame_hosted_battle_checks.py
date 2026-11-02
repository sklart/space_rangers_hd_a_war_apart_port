"""Actual PGOrderAttack and bounded extra original form Takt calls, Windows HAL."""
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT,read

def exclusive_source(path,text):
 if path.exists():assert path.read_text(encoding='utf-8')==text
 else:path.write_text(text,encoding='utf-8')

if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e588-hosted-battle';check.BASE.mkdir(parents=True,exist_ok=True)
 check.NATIVE=ROOT/'build/matrixgame/e2e583-robot-orders/native-build'
 text=(ROOT/'tests/probe_matrixgame_hosted_windows_loop.cpp').read_text(encoding='utf-8')
 replacements={
  '#include "init_runtime.hpp"':'#include "init_runtime.hpp"\n#include "matrixgame_battle_probe.hpp"',
  ' native_input::EngineSink input;platform3g::AppletEngineEvents events;':' native_input::EngineSink input;platform3g::AppletEngineEvents events;BattleProbe battle;unsigned extra_ticks{};',
  '  case 209:g_Flags|=GFLAG_EXITLOOP;break;':'  case 9:battle.Issue();break;\n  case 209:battle.Verify();g_Flags|=GFLAG_EXITLOOP;break;',
  'default:assert(pumps<209);assert(!g_MatrixMap->IsPaused());break;':'default:assert(pumps<209);assert(!g_MatrixMap->IsPaused());break;',
  ' }\n void Idle()override':'  if(pumps>=9 && pumps<209)for(unsigned i=0;i<20 && battle.Continue();++i){g_FormCur->Takt(25);++extra_ticks;}\n }\n void Idle()override',
  'backend.draws==208 && g_MatrixMap->GetTime()-backend.start_time==5125':'backend.draws==208 && g_MatrixMap->GetTime()-backend.start_time==5125+int(backend.extra_ticks)*25',
  '    std::puts("HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125 input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS");':'    std::printf("HOSTED_BATTLE_LOOP draws=208 loop_elapsed=5125 extra_form_Takt25=%u map_elapsed=%d input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS\\n",backend.extra_ticks,g_MatrixMap->GetTime()-backend.start_time-25);',
  'E581-Windows-HAL-hosted-loop':'E588-Windows-HAL-hosted-battle',
 }
 for before,after in replacements.items():assert text.count(before)==1;text=text.replace(before,after,1)
 probe=check.BASE/'battle_loop.cpp';exclusive_source(probe,text)
 original=check.NATIVE/'native/source/MatrixGame/src/Effects/MatrixEffectWeapon.cpp';text=original.read_text(encoding='utf-8')
 for line in ('            odead = hiti->Damage(w->m_Type, pos, dir, w->GetSideStorage(), w->GetOwner());','            odead = hiti->Damage(w->m_Type, pos, w->m_Dir, w->GetSideStorage(), w->GetOwner());'):
  assert text.count(line)==1;text=text.replace(line,'            auto battle_hit=BeginBattleHit(hiti,w->m_Owner,int(w->m_Type));\n'+line+'\n            EndBattleHit(battle_hit,odead);',1)
 observed=check.BASE/'observed_battle_weapon.cpp';exclusive_source(observed,'#include "matrixgame_battle_probe.hpp"\n'+text)
 check.PROBE=probe;check.EXTRA_SOURCES=[ROOT/'tests/matrixgame_battle_probe.cpp',observed]
 check.EXTRA_SOURCES+=[ROOT/'port/matrixgame/native-engine'/name for name in ('native_form_session.cpp','native_form_backend.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp')]
 check.EXTRA_SOURCES+=[check.NATIVE/'native/source/MatrixLib/3G/OriginalNativeLoop.cpp',check.NATIVE/'native/source/MatrixGame/src/OriginalNativeForm.cpp']
 check.EXTRA_FLAGS=['/I'+str(ROOT/'tests'),'/I'+str(original.parent)];check.main()
 records=sorted(check.BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);record=read(records[0]);assert record['status'].startswith('PASS')
 assert record['stdout'].count('BATTLE original_attack_navigation_weapon_virtual_Damage_HP=PASS')==2
