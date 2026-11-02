"""Original robot Fire/StopFire, weapon effects and callbacks in a genuine world."""
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT,read
if __name__=='__main__':
 check.BASE=ROOT/'build/matrixgame/e2e585-hosted-firing';check.BASE.mkdir(parents=True,exist_ok=True);check.NATIVE=ROOT/'build/matrixgame/e2e583-robot-orders/native-build'
 text=(ROOT/'tests/probe_matrixgame_hosted_windows_loop.cpp').read_text();replacements={
  '#include "init_runtime.hpp"':'#include "init_runtime.hpp"\n#include "matrixgame_firing_probe.hpp"',
  ' native_input::EngineSink input;platform3g::AppletEngineEvents events;':' native_input::EngineSink input;platform3g::AppletEngineEvents events;FiringProbe firing;',
  '  case 209:g_Flags|=GFLAG_EXITLOOP;break;':'  case 9:firing.Issue();break;\n  case 169:firing.Stop();break;\n  case 209:firing.Verify();g_Flags|=GFLAG_EXITLOOP;break;',
  'E581-Windows-HAL-hosted-loop':'E585-Windows-HAL-hosted-firing',
 }
 for before,after in replacements.items():assert text.count(before)==1;text=text.replace(before,after,1)
 probe=check.BASE/'firing_loop.cpp'
 if probe.exists():assert probe.read_text()==text
 else:probe.write_text(text,encoding='utf-8')
 original=check.NATIVE/'native/source/MatrixGame/src/Effects/MatrixEffectWeapon.cpp';text=original.read_text(encoding='utf-8')
 hooks={'    ++m_FireCount;':'    ++m_FireCount;\n    ObserveOriginalFire(*this,m_Owner);','    CMatrixEffectWeapon *w = (CMatrixEffectWeapon *)user;':'    CMatrixEffectWeapon *w = (CMatrixEffectWeapon *)user;\n    ObserveOriginalHit(*w,w->m_Owner,hiti,flags);'}
 for before,after in hooks.items():assert text.count(before)==1;text=text.replace(before,after,1)
 text='#include "matrixgame_firing_probe.hpp"\n'+text;observed=check.BASE/'observed_weapon.cpp'
 if observed.exists():assert observed.read_text(encoding='utf-8')==text
 else:observed.write_text(text,encoding='utf-8')
 check.PROBE=probe;check.EXTRA_SOURCES=[ROOT/'tests/matrixgame_firing_probe.cpp',observed]
 check.EXTRA_SOURCES+=[ROOT/'port/matrixgame/native-engine'/name for name in ('native_form_session.cpp','native_form_backend.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp')]
 check.EXTRA_SOURCES+=[check.NATIVE/'native/source/MatrixLib/3G/OriginalNativeLoop.cpp',check.NATIVE/'native/source/MatrixGame/src/OriginalNativeForm.cpp']
 check.EXTRA_FLAGS=['/I'+str(ROOT/'tests'),'/I'+str(original.parent)];check.main()
 records=sorted(check.BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);record=read(records[0]);assert record['status'].startswith('PASS')
 assert record['stdout'].count('original_callbacks=PASS counter_consistency=PASS stop_effective=PASS live_robot=PASS')==2
