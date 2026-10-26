// SPDX-License-Identifier: GPL-2.0-or-later
// Original MouseKey/selection/render/recovery on actual HAL. Controlled events,
// not OS HID or a claim about Switch controller hardware.
#include "MatrixFormGame.hpp"
#include "MatrixMap.hpp"
#include "MatrixSide.hpp"
#include "MatrixMultiSelection.hpp"
#include "MatrixMapStatic.hpp"
#include "MatrixObjectRobot.hpp"
#include "Logic/MatrixAIGroup.h"
#include "Interface/CInterface.h"
#include "native_input_engine.hpp"
#include "applet_engine_events.hpp"
#include "platform_3g.hpp"
#include <cstdio>
#include <stdexcept>
#include <cmath>
namespace {
namespace mg=srhd_awa::matrixgame;
void Need(bool value,const char* text){if(!value)throw std::runtime_error(text);}
}
void RunOriginalSelectionScenario(mg::platform3g::Device&,CFormMatrixGame& form){
 using namespace mg;native_input::EngineSink sink;native_input::Router router(sink);platform3g::AppletEngineEvents events;
 router.Extent(800,600);router.Focus(true);std::uint64_t now=1000;unsigned frames{};const auto start=g_MatrixMap->GetTime();
 auto feed=[&](native_input::Source source,unsigned buttons,int x,int y,bool point=true){native_input::Sample sample;sample.buttons=buttons;sample.point=point;sample.x=x;sample.y=y;router.Feed(source,sample,now+=25);};
 auto frame=[&]{form.Takt(25);form.Draw();++frames;};
 auto drain=[&]{for(unsigned i=0;i<3;++i)frame();}; // Original DIP deletion after 50ms.
 Need(!g_MatrixMap->IsPaused() && !g_MatrixMap->GetPlayerSide()->IsArcadeMode(),"selection requires actual unpaused RTS");
 bool world=false;for(auto xy:{Base::CPoint(400,300),Base::CPoint(350,250),Base::CPoint(450,250),Base::CPoint(550,250)}){
  feed(native_input::Source::Mouse,0,xy.x,xy.y);frame();if(g_IFaceList->m_InFocus==UNKNOWN){world=true;break;}}
 Need(world,"no actual world focus at bounded selection test positions");
 CMatrixMapStatic* target{};int x{},y{},right{},bottom{};D3DXMATRIX identity;D3DXMatrixIdentity(&identity);
 for(unsigned pass=0;pass<2 && !target;++pass)for(int i=0;i<CMatrixMapStatic::GetVisObjCnt();++i){
  auto* object=CMatrixMapStatic::GetVisObj(i);if(object->GetSide()!=PLAYER_SIDE)continue;
  if(pass==0?(!object->IsLiveRobot() || object->AsRobot()->IsCrazy()):!object->IsLiveBuilding())continue;
  const auto screen=g_MatrixMap->m_Camera.Project(object->GetGeoCenter(),identity);
  if(!std::isfinite(screen.x) || !std::isfinite(screen.y) || screen.x<240 || screen.x>560 || screen.y<160 || screen.y>340)continue;
  const int left=int(screen.x)-60,top=int(screen.y)-60;
  feed(native_input::Source::Mouse,0,left,top);frame();if(g_IFaceList->m_InFocus!=UNKNOWN)continue;
  target=object;x=left;y=top;right=int(screen.x)+60;bottom=int(screen.y)+60;break;
 }
 Need(target!=nullptr,"no real visible player object in bounded world selection area");
 feed(native_input::Source::Mouse,1,x,y);Need(CMultiSelection::m_GameSelection!=nullptr,"original MouseKey did not begin mouse drag");frame();
 feed(native_input::Source::Mouse,1,(x+right)/2,(y+bottom)/2);Need(CMultiSelection::m_GameSelection!=nullptr,"original MouseMove lost drag");frame();
 feed(native_input::Source::Mouse,1,right,bottom);frame();Need(CMultiSelection::m_GameSelection->FindItem(target),"original selection geometry did not include real player object");
 feed(native_input::Source::Mouse,0,0,0,false);Need(!CMultiSelection::m_GameSelection,"original MouseKey did not end mouse drag");drain();
 auto* side=g_MatrixMap->GetPlayerSide();Need(target->IsBuilding()?side->m_ActiveObject==target:(side->GetCurGroup() && side->GetCurGroup()->FindObject(target)),"original MouseKey release did not publish selected player object");
 const int selected_type=int(target->GetObjectType());now+=600; // Independent gestures, outside native double-click window.
 Need(target->IsLiveRobot(),"direct control scenario requires the actually selected live player robot");
 // Original Keyboard chooses the group's current object, then EnterRobot(false)
 // enters that one-object group. A drag can select more than the anchor robot.
 auto* expected_arcade=side->GetCurGroup()->GetObjectByN(side->GetCurSelNum());
 Need(expected_arcade && expected_arcade->IsLiveRobot(),"original current group object is not a live robot");
 native_input::Sample keyboard;
 auto action=[&](EKeyAction key,bool down){const auto vk=g_Config.m_KeyActions[key];Need(vk!=0,"unbound original direct control action");keyboard.keys.set(vk,down);router.Feed(native_input::Source::Keyboard,keyboard,now+=25);};
 action(KA_UNIT_ENTER,true);action(KA_UNIT_ENTER,false);
 std::printf("REAL_ARCADE_ENTRY robot_mode=%u expected=%p actual=%p selection=%d ordering=%u\n",unsigned(side->IsRobotMode()),expected_arcade,side->GetArcadedObject(),int(side->m_CurrSel),unsigned(bool(g_IFaceList->m_IfListFlags&ORDERING_MODE)));
 Need(side->IsRobotMode() && side->GetArcadedObject()==expected_arcade,"original Keyboard did not enter current selected robot");frame();
 auto* robot=expected_arcade->AsRobot();const float start_x=robot->m_PosX,start_y=robot->m_PosY;
 action(KA_UNIT_FORWARD,true);Need(robot->FindOrderLikeThat(ROT_MOVE_TO),"original forward key did not create move order");
 for(unsigned i=0;i<8;++i)frame();action(KA_UNIT_FORWARD,false);frame();
 const float movement=std::hypot(robot->m_PosX-start_x,robot->m_PosY-start_y);Need(movement>0.01f,"original direct-control robot did not move");
 feed(native_input::Source::Mouse,0,400,250);frame();Need(g_IFaceList->m_InFocus==UNKNOWN,"fire target is over interface");
 Need(g_Config.m_KeyActions[KA_FIRE]==1,"source fire alias scenario requires original VK_LBUTTON binding");
 feed(native_input::Source::Mouse,1,400,250);frame();Need(robot->FindOrderLikeThat(ROT_FIRE),"original mouse fire did not create fire order");
 native_input::Sample pad_fire;pad_fire.keys.set(g_Config.m_KeyActions[KA_FIRE]);router.Feed(native_input::Source::Pad,pad_fire,now+=25);frame();
 router.Feed(native_input::Source::Pad,{},now+=25);frame();
 const bool held_fire=native_input::ReadEngineKeys().test(1),fire_order=robot->FindOrderLikeThat(ROT_FIRE);
 std::printf("REAL_ARCADE enter=PASS forward_order=PASS movement=%f mouse_fire=PASS mouse_held_after_pad_key_release=%u fire_order_after_pad_key_release=%u\n",movement,unsigned(held_fire),unsigned(fire_order));
 feed(native_input::Source::Mouse,0,0,0,false);frame();Need(!robot->FindOrderLikeThat(ROT_FIRE),"original mouse release did not stop fire");
 action(KA_UNIT_ENTER,true);action(KA_UNIT_ENTER,false);frame();Need(!side->IsArcadeMode(),"original Keyboard did not leave robot");
 Need(held_fire && fire_order,"releasing pad fire key interrupted mouse-owned original fire");now+=600;
 // The same logical left button is held by two real provider sources. Releasing
 // the mouse must not deliver an Up while the pad still owns the hold.
 feed(native_input::Source::Mouse,0,360,250);frame();Need(g_IFaceList->m_InFocus==UNKNOWN,"merged drag starts over interface");
 feed(native_input::Source::Mouse,1,360,250);auto* merged=CMultiSelection::m_GameSelection;Need(merged!=nullptr,"merged drag did not begin");frame();
 feed(native_input::Source::Pad,1,400,280);frame();
 feed(native_input::Source::Mouse,0,0,0,false);Need(CMultiSelection::m_GameSelection==merged,"mouse release cancelled pad-owned drag");
 feed(native_input::Source::Pad,1,450,330);frame();
 feed(native_input::Source::Pad,0,0,0,false);Need(!CMultiSelection::m_GameSelection,"pad release did not end merged drag");drain();
 now+=600;
 // Match SwitchWindow::Events ordering: logical releases precede deactivation.
 feed(native_input::Source::Mouse,0,410,270);frame();Need(g_IFaceList->m_InFocus==UNKNOWN,"focus drag starts over interface");
 feed(native_input::Source::Mouse,1,410,270);Need(CMultiSelection::m_GameSelection!=nullptr,"focus drag did not begin");frame();
 feed(native_input::Source::Mouse,1,465,315);frame();
 router.Focus(false);Need(!CMultiSelection::m_GameSelection,"focus release left original drag active");
 events.FocusChanged(false);Need(!(g_Flags&GFLAG_APPACTIVE) && !(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY),"original selection deactivation did not release resources");
 router.Focus(true);events.FocusChanged(true);frame();Need(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY,"original selection activation did not restore resources");drain();
 router.Focus(false);Need(!CMultiSelection::m_GameSelection && native_input::ReadEngineKeys().none(),"selection input state leaked");
 Need(g_MatrixMap->GetTime()-start==int(frames*25),"selection simulation time mismatch");
 std::printf("REAL_SELECTION mouse_drag=PASS real_player_object=PASS selected_type=%d merged_mouse_pad=PASS focus_release_restore=PASS frames=%u elapsed=%d IzvratMS=%u CONTROLLED_EVENTS NO_OS_HID NO_SWITCH_HARDWARE\n",selected_type,frames,g_MatrixMap->GetTime()-start,unsigned(g_Config.m_IzvratMS));
}
