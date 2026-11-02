"""Original government config/tree/layout in the real E593 Windows caller context."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
BASE=ROOT/'build/matrixgame/e2e594-government-session'
if __name__=='__main__':
 out=BASE/'fixture-depth';out.mkdir(parents=True,exist_ok=True)
 controller=(ROOT/'tests/matrixgame_rangers_frun_reference.inc').read_text(encoding='utf-8')
 controller='#include "rangers_government_session.hpp"\n#include "units/fGov.hpp"\n#include "units/Globals.hpp"\n#include "units/aGalaxy.hpp"\n#include "units/aPlayer.hpp"\n#include "types/GI_GraphButton.hpp"\n'+controller
 controller=controller[:controller.index('  Driver backend;native_loop::Scope loop(backend);')]+r'''
  const auto* style_before=GR_Main::UiStyleConfig;const auto* data_before=GR_Main::GameDataConfig;const auto* depth_before=GR_Main::UiDepthConfig;
  assert(!Globals::GovernmentScreen && !GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]);
  for(int cycle=0;cycle<2;++cycle){
   {
    RangersGovernmentSession government;auto& gov=government.Screen();
    assert(Globals::GovernmentScreen==&gov && GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]==&gov);
    assert(gov.RootUiObject && gov.CursorControl && gov.MainPanel && gov.PlanetPanel && gov.LoadPanel);
    assert(gov.RootUiObject->MessageLoop==&gov && gov.ContentPanel->MessageLoop==&gov);
    assert(pas::assigned(gov.GetByName(u"MainPanel"sv)->KeyDownCallback));
    auto* message=pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(gov.GetByName(u"UserMsgAdd"sv));assert(pas::assigned(message->UpCallback));
    assert(gov.GetByName(u"PM_EndTurn"sv) && gov.GetByName(u"PM_Ship"sv) && gov.GetByName(u"TalkPA"sv) && gov.GetByName(u"PP_Gov"sv));
    std::size_t count=0;const auto visit=[&](auto&& self,GI_MessageLoop::TObjectGI* p)->void {for(;p;p=p->NextSibling){assert(++count<100000);if(p->FirstChild)self(self,p->FirstChild);}};
    visit(visit,gov.RootUiObject);assert(count>100);
    bool duplicate=false;try{RangersGovernmentSession other;}catch(const std::logic_error&){duplicate=true;}assert(duplicate && Globals::GovernmentScreen==&gov);
    assert(!aGalaxy::Galaxy && !aPlayer::GetPlayer());
    std::printf("GOVERNMENT_LAYOUT cycle=%d controls=%zu original_Gov_constructor_config_layout=PASS real_callbacks=PASS duplicate_owner=REJECTED OnOpen_Galaxy=NOT_RUN\n",cycle,count);
   }
   assert(!Globals::GovernmentScreen && !GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]);
   assert(GR_Main::UiStyleConfig==style_before && GR_Main::GameDataConfig==data_before && GR_Main::UiDepthConfig==depth_before);
  }
  GR_Main::GR_DXReset();VerifyCallerGammaAfterBattle();
  Robot::FinalizeRobotRuntime();assert(!Robot::RobotInterface && !g_RangersInterface && !EngineCallbacks::Bound());
 }
 HostClean();MatrixClean();journal.RequireHealthy();std::puts("GOVERNMENT_LAYOUT original_destructors_root_restore_host_cleanup=PASS no_save_load=PASS");return 0;
}
'''
 # Raw fixture text uses normal C++ quotes/newlines, never escaped shell content.
 controller=controller.replace(r'\"','"').replace(r'\\n',r'\n')
 path=out/'government_reference.inc'
 if path.exists():assert path.read_text(encoding='utf-8')==controller
 else:path.write_text(controller,encoding='utf-8')
 text=(ROOT/'tools/matrixgame_rangers_frun_windows.py').read_text(encoding='utf-8')
 changes={
  "BASE=BASE.parent/('reset-only-gamma' if args.reset_only else 'frun-windows-asan-gamma')":"BASE=ROOT/'build/matrixgame/e2e594-government-session/windows-asan-depth'",
  "ROOT/'tests/matrixgame_rangers_frun_reference.inc'":"ROOT/'build/matrixgame/e2e594-government-session/fixture-depth/government_reference.inc'",
  "ROOT/'port/matrixgame/native-engine/rangers_legacy_presentation_win32.cpp']":"ROOT/'port/matrixgame/native-engine/rangers_legacy_presentation_win32.cpp',ROOT/'port/matrixgame/native/rangers_government_session.cpp']",
  "else:assert 'original_caller_stats=PASS sound_music_restore=PASS original_GI_invalidation=PASS cwd_loose_root_cleanup=PASS Galaxy_return=NOT_RUN' in result['stdout']":"else:assert result['stdout'].count('original_Gov_constructor_config_layout=PASS real_callbacks=PASS duplicate_owner=REJECTED OnOpen_Galaxy=NOT_RUN')==2 and 'original_destructors_root_restore_host_cleanup=PASS no_save_load=PASS' in result['stdout']",
  'PASS; ORIGINAL FRUN FULL MISSION/STATISTICS/SDK RESET/GI INVALIDATION; BASE GI CALLER ONLY; GALAXY NOT RUN':'PASS; ORIGINAL GOVERNMENT UI CONFIG/TREE/LAYOUT/CLEANUP; ONOPEN/GALAXY NOT RUN',
 }
 for before,after in changes.items():
  expected=2 if before=="ROOT/'tests/matrixgame_rangers_frun_reference.inc'" else 1
  assert text.count(before)==expected,before;text=text.replace(before,after)
 text='import sys\nsys.path.insert(0,'+repr(str(ROOT/'tools'))+')\n'+text
 script=out/'run.py'
 if script.exists():assert script.read_text(encoding='utf-8')==text
 else:script.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,script],check=True)
