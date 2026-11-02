"""Bounded real mission simulation and natural win/loss UI, without state injection."""
import subprocess
import sys
import matrixgame_hosted_windows_map_checks as check
from matrixgame_standard_nro_audit import ROOT,read
BASE=ROOT/'build/matrixgame/e2e590-completed-battle/fixed'
if __name__=='__main__':
 check.BASE=BASE;BASE.mkdir(parents=True,exist_ok=True);check.NATIVE=ROOT/'build/matrixgame/e2e583-robot-orders/native-build'
 generated=BASE/'generated';subprocess.run([sys.executable,ROOT/'tools/matrixgame_engine_prepare.py','--output',generated],check=True)
 flame=generated/'MatrixGame/src/Effects/MatrixEffectFlame.cpp'
 original_flame=flame.read_text(encoding='utf-8')
 cookie_probe=r'''
namespace {uintptr_t observed_flame_cookie;}
void CheckOriginalFlameCookie(CMatrixMapStatic* object){
 assert(object && object->IsRobot());const auto expected=reinterpret_cast<uintptr_t>(object);
 assert(expected>0xffffffffu);FlameData data{};static_assert(sizeof(data.uvalue)==sizeof(uintptr_t));
 data.handler=[](CMatrixMapStatic*,const D3DXVECTOR3&,uintptr_t value,std::uint32_t){observed_flame_cookie=value;};
 data.uvalue=expected;assert(FlameEnum(object->GetGeoCenter(),object,reinterpret_cast<uintptr_t>(&data)));
 assert(observed_flame_cookie==expected);std::puts("COMPLETION original_FlameEnum_high_pointer_cookie=PASS");
}
'''
 tested_flame=BASE/'tested_flame.cpp';flame_text='#include <cassert>\n#include <cstdio>\n'+original_flame+cookie_probe
 if tested_flame.exists():assert tested_flame.read_text(encoding='utf-8')==flame_text
 else:tested_flame.write_text(flame_text,encoding='utf-8')
 sound=(ROOT/'build/matrixgame/e2e577-rangers-windows/source/EC_CacheSound.cpp').read_text(encoding='utf-8')
 parse_before='            static_cast<std::size_t>(SourceBuffer->Position));'
 assert sound.count(parse_before)==1
 sound=sound.replace(parse_before,'            static_cast<std::size_t>(SourceBuffer->Position),\n            srhd_awa::platform::e2e4_wav::RiffSizePolicy::LegacyTotalBytes);',1)
 before='        if (!wave) {'
 assert sound.count(before)==1
 sound=sound.replace(before,before+r'''
            std::fprintf(stderr,"COMPLETION_WAVE_ERROR parser=%s bytes=%d position=%d key=",wave.error,SourceBuffer->DataSize,SourceBuffer->Position);
            for(auto ch:pas::view(CacheKey))std::fputc(ch<128?int(ch):'?',stderr);
            std::fputc('\n',stderr);std::fflush(stderr);
            assert(SourceBuffer->DataSize>0 && SourceBuffer->DataSize<64*1024*1024);
            FILE* dump=std::fopen("output/failed-wave-next.bin","wbx");assert(dump);
            assert(std::fwrite(SourceBuffer->Data,1,SourceBuffer->DataSize,dump)==std::size_t(SourceBuffer->DataSize));assert(std::fclose(dump)==0);
''',1)
 observed_sound=BASE/'observed_sound_legacy.cpp';sound='#include <cassert>\n#include <cstdio>\n'+sound
 if observed_sound.exists():assert observed_sound.read_text(encoding='utf-8')==sound
 else:observed_sound.write_text(sound,encoding='utf-8')
 text=(ROOT/'tests/probe_matrixgame_hosted_windows_loop.cpp').read_text(encoding='utf-8')
 helpers=r'''
 unsigned extra_ticks{},issued_groups{};int natural_outcome{};
 void Issue(){
  std::set<int> groups;
  bool checked_cookie=false;
  for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic())
   if(p->IsLiveRobot() && p->GetSide()==PLAYER_SIDE && !p->AsRobot()->GetBase()){
    if(!checked_cookie){CheckOriginalFlameCookie(p);checked_cookie=true;}
    groups.insert(g_MatrixMap->GetPlayerSide()->RobotToLogicGroup(p->AsRobot()));
   }
  assert(!groups.empty());
  for(int group:groups){assert(group>=0 && group<MAX_LOGIC_GROUP);g_MatrixMap->GetPlayerSide()->PGOrderAutoAttack(group);}
  issued_groups=unsigned(groups.size());std::printf("COMPLETION_COMMAND original_auto_attack_groups=%u map_time=%d\n",issued_groups,g_MatrixMap->GetTime());
 }
 void Inventory(){
  unsigned player{},hostile{},buildings{},player_bases{};
  for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic()){
   if(p->IsLiveRobot()){if(p->GetSide()==PLAYER_SIDE)++player;else if(p->GetSide()>0)++hostile;}
   if(p->IsLiveBuilding()){++buildings;if(p->IsBase() && p->GetSide()==PLAYER_SIDE)++player_bases;}
  }
  std::printf("COMPLETION_PROGRESS ticks=%u map_time=%d players=%u hostile_robots=%u buildings=%u player_bases=%u flags=%u\n",extra_ticks,g_MatrixMap->GetTime(),player,hostile,buildings,player_bases,unsigned(g_MatrixMap->m_Flags));std::fflush(stdout);
 }
 bool NaturalExit(){
  const auto* name=g_MatrixMap->m_DialogModeName;if(!name)return false;
  const bool win=std::wcscmp(name,TEMPLATE_DIALOG_WIN)==0,loss=std::wcscmp(name,TEMPLATE_DIALOG_LOOSE)==0;
  if(!win && !loss)throw std::runtime_error("unexpected dialog during bounded mission simulation");
  assert(g_MatrixMap->IsPaused());natural_outcome=win?3:2;
  assert(bool(g_MatrixMap->m_Flags&MMFLAG_WIN)==win);
  Inventory();std::printf("COMPLETION_NATURAL_DIALOG state=%d map_time=%d ticks=%u\n",natural_outcome,g_MatrixMap->GetTime(),extra_ticks);
  Key(13);assert(g_ExitState==natural_outcome);g_FormCur->Takt(25);
  assert(g_MatrixMap->m_DialogModeName && std::wcscmp(g_MatrixMap->m_DialogModeName,TEMPLATE_DIALOG_STATISTICS)==0);
  Key(13);assert(g_ExitState==natural_outcome && (g_Flags&GFLAG_EXITLOOP));
  std::puts("COMPLETION natural_win_loss_statistics_original_input_exit=PASS");return true;
 }
'''
 advance=r'''
  if(pumps>=9){
   if(pumps==9){Issue();Inventory();}
   for(unsigned i=0;i<200;++i){
    if(NaturalExit())return;
    if(g_MatrixMap->IsPaused())throw std::runtime_error("unexpected pause before mission outcome");
    if(extra_ticks>=120000){Inventory();throw std::runtime_error("natural mission outcome not reached within 3000000 ms extra simulation");}
    g_FormCur->Takt(25);++extra_ticks;if(extra_ticks%2000==0)Inventory();
   }
   return;
  }
'''
 result=r'''
   SRobotGameState saved{};runtime.SaveOriginalGameResult(saved);
   const auto returned=ReadEngineStatistics(saved);
   assert(g_ExitState==2 || g_ExitState==3);
   WriteStatistics(returned,Robot::RobotBattleStatistics);
   const auto host_result=ReadStatistics(Robot::RobotBattleStatistics);
   assert(host_result.signed_time_ms==saved.m_Time && host_result.robots_built==saved.m_BuildRobot && host_result.robots_destroyed==saved.m_KillRobot && host_result.turrets_built==saved.m_BuildTurret && host_result.turrets_destroyed==saved.m_KillTurret && host_result.buildings_destroyed==saved.m_KillBuilding);
   std::printf("COMPLETION_RESULT state=%d signed_time=%d robots_built=%d robots_destroyed=%d turrets_built=%d turrets_destroyed=%d buildings_destroyed=%d original_Robot_global_statistics=PASS galaxy_screen_transition=NOT_RUN\n",g_ExitState,host_result.signed_time_ms,host_result.robots_built,host_result.robots_destroyed,host_result.turrets_built,host_result.turrets_destroyed,host_result.buildings_destroyed);
'''
 replacements={
  '#include "init_runtime.hpp"':'#include "init_runtime.hpp"\n#include "MatrixSide.hpp"\n#include "MatrixRobot.hpp"\n#include "StringConstants.hpp"\n#include "engine_callbacks.hpp"\n#include "units/Robot.hpp"\n#include "types/aGalaxyStruct.hpp"\n#include <set>\n#include <cwchar>\nvoid CheckOriginalFlameCookie(CMatrixMapStatic*);',
  ' void Pump()override{':helpers+'\n void Pump()override{',
  '  ++pumps;':'  ++pumps;'+advance,
  '  case 209:g_Flags|=GFLAG_EXITLOOP;break;':'',
  'native_loop::Scope scope(backend);g_Flags&=~GFLAG_EXITLOOP;':'native_loop::Scope scope(backend);g_ExitState=0;g_Flags&=~GFLAG_EXITLOOP;',
  'backend.draws==208 && g_MatrixMap->GetTime()-backend.start_time==5125':'backend.draws>=8 && backend.natural_outcome==g_ExitState && (g_ExitState==2 || g_ExitState==3)',
  '    std::puts("HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125 input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS");':'    std::printf("COMPLETION_LOOP draws=%u pumps=%u extra_ticks=%u outcome=%d focus_recovery=PASS\\n",backend.draws,backend.pumps,backend.extra_ticks,backend.natural_outcome);',
  '   runtime.Deinit();assert(runtime.LastStateRestoreResult()':result+'\n   runtime.Deinit();assert(runtime.LastStateRestoreResult()',
  'E581-Windows-HAL-hosted-loop':'E590-Windows-HAL-completed-battle',
  '  catch(const std::exception& error)':'''  catch(const pas::Raised& raised){const auto* e=pas::class_cast_if<pas::Exception*>(raised.object.get());std::printf("COMPLETION_HOST_EXCEPTION message=%s\\n",e?reinterpret_cast<const char*>(e->message.data()):"non-Exception Pascal object");}
  catch(const std::exception& error)''',
 }
 for before,after in replacements.items():assert text.count(before)==1;text=text.replace(before,after,1)
 probe=BASE/'completed_loop_legacy_audio.cpp'
 if probe.exists():assert probe.read_text(encoding='utf-8')==text
 else:probe.write_text(text,encoding='utf-8')
 check.PROBE=probe;check.EXTRA_SOURCES=[tested_flame,observed_sound,*[ROOT/'port/matrixgame/native-engine'/name for name in ('original_game.cpp','native_form_session.cpp','native_form_backend.cpp','native_loop.cpp','native_input_engine.cpp','applet_engine_events.cpp')]]
 check.EXCLUDED_HOST_OBJECTS=['EC_CacheSound.obj']
 # All unchanged headers must come from the same tree as the reused archives:
 # mixing equal pragma-once headers from two absolute roots duplicates types.
 check.EXTRA_FLAGS=['/I'+str(check.NATIVE/'native/source/MatrixGame/src/Effects')]
 check.EXTRA_SOURCES+=[check.NATIVE/'native/source/MatrixLib/3G/OriginalNativeLoop.cpp',check.NATIVE/'native/source/MatrixGame/src/OriginalNativeForm.cpp'];check.main()
 record=read(max(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));assert record['status'].startswith('PASS')
 assert record['stdout'].count('COMPLETION natural_win_loss_statistics_original_input_exit=PASS')==2
