// SPDX-License-Identifier: GPL-2.0-or-later
// Executes production CPU provider logic and original CForm/Input. No HID/GPU emulation.
#include "native_cursor.hpp"
#include "native_input_engine.hpp"
#include "Form.hpp"
#include "input.hpp"
#include "MatrixConfig.hpp"
#include <cassert>
#include <cstdlib>
#include <new>
#include <cstdio>
#include <tuple>
#include <vector>
namespace {int fail_after=-1;}
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
std::uint32_t test_flags=1;std::uint32_t& g_Flags=test_flags;
CMatrixConfig g_Config;
namespace c=srhd_awa::matrixgame::native_cursor;
namespace i=srhd_awa::matrixgame::native_input;
struct Form final:CForm {
    std::vector<std::pair<bool,unsigned>> keys;std::vector<std::tuple<ButtonStatus,int,int,int>> mouse;std::vector<std::pair<int,int>> points;
    void Enter()override{}void Leave()override{}void Draw()override{}void Takt(int)override{}
    void MouseMove(int x,int y)override{points.emplace_back(x,y);}
    void MouseKey(ButtonStatus type,int key,int x,int y)override{if(type==B_DOWN)Input::onKeyDown(std::uint8_t(key));else if(type==B_UP)Input::onKeyUp(std::uint8_t(key));mouse.emplace_back(type,key,x,y);}
    void Keyboard(bool down,std::uint8_t key)override{assert(Input::isVKeyPressed(key)==down);keys.emplace_back(down,key);}
    void SystemEvent(ESysEvent)override{}
};
template<class F>void Fails(F operation){bool threw=false;try{operation();}catch(const std::exception&){threw=true;}assert(threw);}
int main(){
    c::Store cursors,foreign;std::vector<std::byte> rgba(4*2*3);
    for(unsigned p=0;p<6;++p){rgba[p*4]=std::byte(10+p);rgba[p*4+1]=std::byte(20+p);rgba[p*4+2]=std::byte(30+p);rgba[p*4+3]=std::byte(p*51);}
    const auto a=cursors.Create(2,3,1,2,rgba);assert(cursors.Count()==1 && !cursors.Visible());
    assert(cursors.Exchange(a)==0 && cursors.Read().selected==a);auto held=cursors.Visible();assert(held && held->rgba==rgba);
    const auto bgra=c::Bgra(*held);for(unsigned p=0;p<6;++p){assert(bgra[p*4]==rgba[p*4+2] && bgra[p*4+2]==rgba[p*4] && bgra[p*4+3]==rgba[p*4+3]);}
    cursors.Position(10,20);const auto quad=c::Quad(*held,cursors.Read());
    assert(quad[0].x==8.5f && quad[0].y==20.5f && quad[1].y==17.5f && quad[3].x==10.5f && quad[3].u==1 && quad[3].v==0);
    const auto snapshot=cursors.Read();assert(cursors.Show(false)==-1 && !cursors.Visible());assert(cursors.Show(false)==-2);
    assert(cursors.Show(true)==-1 && !cursors.Visible());assert(cursors.Show(true)==0 && cursors.Visible());
    assert(!cursors.ShowDevice(true));assert(cursors.ShowDevice(false));
    Fails([&]{foreign.Exchange(a);});Fails([&]{foreign.Destroy(a);});Fails([&]{cursors.Destroy(a);});
    Fails([&]{cursors.Create(2,3,2,0,rgba);});Fails([&]{cursors.Create(2,3,0,0,std::span{rgba}.first(2));});
    const auto b=cursors.Create(2,3,0,0,rgba);assert(cursors.Exchange(b)==a);cursors.Restore(snapshot);assert(cursors.Read().selected==a);
    cursors.Destroy(b);Fails([&]{cursors.Exchange(b);});
    unsigned oom=0;for(int allocation=0;allocation<3;++allocation){fail_after=allocation;try{(void)cursors.Create(2,3,0,0,rgba);assert(false);}catch(const std::bad_alloc&){++oom;}fail_after=-1;assert(cursors.Count()==1 && cursors.Read().selected==a);}
    cursors.Exchange(0);cursors.Destroy(a);assert(cursors.Count()==0 && held->rgba==rgba);Fails([&]{cursors.Restore(snapshot);});held.reset();cursors.Destroy(0);
    CForm::StaticInit();Form form;g_FormCur=&form;i::EngineSink sink;i::Router router(sink);router.Extent(1280,720);router.Focus(true);
    const auto original=i::ReadEngineKeys();i::Sample keyboard;keyboard.keys.set('A');keyboard.keys.set(16);router.Feed(i::Source::Keyboard,keyboard,0);
    assert(Input::isVKeyPressed('A') && form.keys.size()==2);
    g_Config.m_KeyActions[KA_SCROLL_LEFT]='A';assert(Input::isKeyPressed(KA_SCROLL_LEFT));
    i::Sample pad;pad.keys.set('A');router.Feed(i::Source::Pad,pad,1);assert(form.keys.size()==2);
    router.Feed(i::Source::Keyboard,{},2);assert(Input::isVKeyPressed('A') && !Input::isVKeyPressed(16));
    router.Feed(i::Source::Pad,{},3);assert(!Input::isVKeyPressed('A'));
    keyboard.keys.reset(16);router.Feed(i::Source::Keyboard,keyboard,100);auto count=form.keys.size();
    router.Repeat(599);assert(form.keys.size()==count);router.Repeat(600);assert(form.keys.size()==count+1);
    router.Repeat(10000);assert(form.keys.size()==count+5);router.Feed(i::Source::Keyboard,{},10001);
    i::Sample mouse;mouse.point=true;mouse.x=30;mouse.y=40;mouse.buttons=1;router.Feed(i::Source::Mouse,mouse,100);
    assert(form.points.back()==std::make_pair(30,40) && std::get<0>(form.mouse.back())==B_DOWN);
    pad.buttons=1;router.Feed(i::Source::Pad,pad,110);count=form.mouse.size();mouse.buttons=0;router.Feed(i::Source::Mouse,mouse,120);assert(form.mouse.size()==count);
    pad.buttons=0;router.Feed(i::Source::Pad,pad,130);assert(std::get<0>(form.mouse.back())==B_UP);
    mouse.buttons=1;router.Feed(i::Source::Mouse,mouse,140);assert(std::get<0>(form.mouse.back())==B_DOUBLE && !Input::isVKeyPressed(1));
    mouse.buttons=0;router.Feed(i::Source::Mouse,mouse,150);mouse.buttons=1;router.Feed(i::Source::Mouse,mouse,160);assert(std::get<0>(form.mouse.back())==B_DOWN);
    mouse.buttons=0;mouse.wheel=-2;router.Feed(i::Source::Mouse,mouse,170);assert(std::get<0>(form.mouse.back())==B_WHEEL && std::get<1>(form.mouse.back())==-2);
    mouse.wheel=0;mouse.x=-100;mouse.y=800;router.Feed(i::Source::Mouse,mouse,180);assert(router.X()==0 && router.Y()==719);
    i::Sample touch;touch.point=true;touch.x=200;touch.y=300;touch.buttons=1;router.Feed(i::Source::Touch,touch,200);assert(router.X()==200 && router.Y()==300);
    router.Feed(i::Source::Keyboard,keyboard,210);router.Focus(false);
    assert(!Input::isVKeyPressed('A') && std::get<0>(form.mouse.back())==B_UP);
    count=form.keys.size();router.Feed(i::Source::Keyboard,keyboard,220);router.Repeat(1000);assert(form.keys.size()==count);
    router.Focus(true);router.Feed(i::Source::Keyboard,keyboard,1100);assert(Input::isVKeyPressed('A'));
    g_FormCur=nullptr;router.Focus(false);assert(!Input::isVKeyPressed('A'));
    g_FormCur=&form;
    for(unsigned button=0;button<3;++button){
        const unsigned vk=button==0?1:button==1?2:4;router.Focus(true);
        i::Sample mouse_owner,pad_owner;mouse_owner.buttons=1u<<button;pad_owner.keys.set(vk);
        auto key_events=form.keys.size();router.Feed(i::Source::Mouse,mouse_owner,20000);
        assert(Input::isVKeyPressed(vk) && form.keys.size()==key_events); // No synthetic Keyboard.
        router.Feed(i::Source::Pad,pad_owner,20001);auto mouse_events=form.mouse.size();
        router.Feed(i::Source::Pad,{},20002);assert(Input::isVKeyPressed(vk) && form.mouse.size()==mouse_events);
        router.Feed(i::Source::Pad,pad_owner,20003);router.Feed(i::Source::Mouse,{},20004);
        assert(Input::isVKeyPressed(vk) && std::get<0>(form.mouse.back())==B_UP);
        router.Repeat(21000);assert(Input::isVKeyPressed(vk));router.Focus(false);assert(!Input::isVKeyPressed(vk));
        router.Focus(true);router.Feed(i::Source::Pad,pad_owner,22000);router.Reset();assert(!Input::isVKeyPressed(vk));
    }
    test_flags=0;g_FormCur=&form;count=form.keys.size();router.Focus(true);router.Feed(i::Source::Keyboard,keyboard,1200);assert(Input::isVKeyPressed('A') && form.keys.size()==count);
    router.Reset();i::RestoreEngineKeys(original);assert(i::ReadEngineKeys()==original);
    for(unsigned key=4;key<30;++key)assert(i::HidKeyToVirtual(key)=='A'+key-4);
    assert(i::HidKeyToVirtual(79)==39 && i::HidKeyToVirtual(224)==17 && i::HidKeyToVirtual(225)==16 && i::HidKeyToVirtual(255)==0);
    Fails([&]{router.Extent(0,1);});Fails([&]{router.Feed(i::Source(99),{},1);});
    g_FormCur=nullptr;
    std::printf("native cursor/input CPU PASS OOM=%u RGBA_hotspot=checked retained_icon=checked multi_source=checked double_click=checked repeat_focus=checked mouse_key_aliases=3 release_orders=checked original_Form_Input=executed HID_GPU_NOT_RUN\n",oom);
}
