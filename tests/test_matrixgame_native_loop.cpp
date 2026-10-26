// SPDX-License-Identifier: GPL-2.0-or-later
// Executes generated original L3GRun + original Form/Reminder. Scripted
// platform/form fixtures test scheduler semantics; no map/GPU/HID claim.
#include "native_loop.hpp"
#include "platform_3g.hpp"
#include "Form.hpp"
#include "3g.hpp"
#include "CReminder.hpp"
#include "stupid_logger.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>
int g_DrawFPS{};
logger_type lgr{std::clog};
namespace p=srhd_awa::matrixgame::platform3g;
namespace loop=srhd_awa::matrixgame::native_loop;
template<class F>void Fails(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}assert(threw);}
struct Device final:p::Device {
    unsigned queries{};
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("fixture has no GPU caps");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("fixture has no gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{++queries;assert(g_DrawFPS>0);return 9*1024*1024+123;}
    std::unique_ptr<p::DeviceStateLease> CaptureState()override{throw p::Unsupported("fixture has no GPU state");}
};
struct Form final:CForm {
    std::vector<int> deltas;unsigned draws{},enters{},leaves{};bool fail_takt{},fail_draw{},reminder_required{};
    void Enter()override{++enters;}void Leave()override{++leaves;}
    void Takt(int delta)override{if(reminder_required)assert(Base::SRemindCore::gtime>0);deltas.push_back(delta);if(fail_takt)throw std::runtime_error("fixture takt failure");}
    void Draw()override{++draws;if(fail_draw)throw std::runtime_error("fixture draw failure");}
    void MouseMove(int,int)override{}void MouseKey(ButtonStatus,int,int,int)override{}
    void Keyboard(bool,std::uint8_t)override{}void SystemEvent(ESysEvent)override{}
};
struct Step {int ms;std::uint32_t flags;CForm* form;};
struct Backend final:loop::Backend {
    std::vector<Step> steps;std::size_t next{};unsigned idles{},now_calls{};int ms{};bool fail_pump{};
    void Pump()override{if(fail_pump)throw std::runtime_error("fixture pump failure");assert(next<steps.size());auto s=steps[next++];ms=s.ms;g_Flags=s.flags;g_FormCur=s.form;}
    void Idle()override{++idles;}
    void Draw(CForm& form)override{form.Draw();}
    loop::Clock::time_point Now()override{++now_calls;return loop::Clock::time_point{}+std::chrono::milliseconds(ms);}
};
bool Remind(std::uintptr_t param){++*reinterpret_cast<unsigned*>(param);return true;}
int main(){
    CForm::StaticInit();Base::SRemindCore::StaticInit();Device device;
    p::Current().ready=true;p::Current().device=&device;
    Fails([]{loop::Pump();});Fails([]{(void)loop::Now();});
    {
        Form form;FormChange(&form);assert(form.enters==1);unsigned reminders{};
        Base::SRemindCore reminder(Remind,reinterpret_cast<std::uintptr_t>(&reminders));reminder.Use(0);form.reminder_required=true;
        Backend backend;backend.steps={{10,GFLAG_APPACTIVE,&form},{40,GFLAG_APPACTIVE|GFLAG_4SPEED,&form},
            {1040,0,&form},{1050,GFLAG_APPACTIVE,&form},{1055,GFLAG_APPACTIVE,nullptr},{1060,GFLAG_EXITLOOP,&form}};
        {loop::Scope scope(backend);Fails([&]{loop::Scope recursive(backend);});assert(RunOriginalNativeLoop()==1);}
        assert((form.deltas==std::vector<int>{10,100,100}));assert(form.draws==3 && reminders>0);
        assert(backend.idles==2 && backend.now_calls==4 && device.queries==3 && g_AvailableTexMem==9);
        assert(g_DrawFPS>0 && g_DrawFPS<=3);FormChange(nullptr);assert(form.leaves==1);
    }
    assert(!g_FormFirst && !g_FormLast && !g_FormCur);
    Base::SRemindCore::StaticInit();
    {
        Form form;Backend backend;backend.steps={{3,GFLAG_APPACTIVE|GFLAG_4SPEED,&form},{7,GFLAG_APPACTIVE|GFLAG_4SPEED,&form},{9,GFLAG_APPCLOSE,&form}};
        {loop::Scope scope(backend);assert(RunOriginalNativeLoop()==1);}
        assert((form.deltas==std::vector<int>{12,16}));assert(form.draws==2);g_FormCur=nullptr;
    }
    for(unsigned failure=0;failure<3;++failure){
        Form form;form.fail_takt=failure==1;form.fail_draw=failure==2;
        Backend backend;backend.fail_pump=failure==0;backend.steps={{1,GFLAG_APPACTIVE,&form}};const auto queries=device.queries;
        {loop::Scope scope(backend);Fails([]{RunOriginalNativeLoop();});}
        assert(device.queries==queries);assert(form.draws==(failure==2?1u:0u));g_FormCur=nullptr;
        Fails([]{loop::Pump();}); // Scope unwound; a later run can bind a new backend.
    }
    {
        Backend backend;backend.steps={{1,GFLAG_APPCLOSE,nullptr}};
        loop::Scope scope(backend);assert(RunOriginalNativeLoop()==1);assert(backend.now_calls==1);
    }
    p::Current()=p::State{};
    std::cout<<"original L3GRun scheduler PASS: delta/4speed/100ms cap, inactive/no-form, reminder-before-takt, draw/FPS/residency, exit and exception rebind; CPU FIXTURES NO_MAP NO_GPU NO_SWITCH_HARDWARE\n";
}
