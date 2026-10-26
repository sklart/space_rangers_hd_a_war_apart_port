// SPDX-License-Identifier: GPL-2.0-or-later
// IPC model only. Production controller and original CForm are executed;
// this does not execute libnx IPC, a GPU, CGame::Init or a map load.
#include "applet_window_lifecycle.hpp"
#include "applet_engine_events.hpp"
#include "3g.hpp"
#include <cassert>
#include <deque>
#include <cstdio>
#include <vector>
std::uint32_t fixture_flags{};
std::uint32_t& g_Flags=fixture_flags;
namespace a=srhd_awa::matrixgame::applet_window;
struct Form final:CForm {
    std::vector<ESysEvent> events;
    bool fail{};
    void Enter()override{}void Leave()override{}void Draw()override{}void Takt(int)override{}
    void MouseMove(int,int)override{}void MouseKey(ButtonStatus,int,int,int)override{}void Keyboard(bool,std::uint8_t)override{}
    void SystemEvent(ESysEvent event)override{events.push_back(event);if(fail)throw std::runtime_error("observer fixture failure");}
};
struct Backend final:a::Backend {
    bool started{},visible{},start_fail{},read_fail{},show_fail{},pump_fail{};
    unsigned starts{},stops{},shows{},restores{},reads{},waits{};std::uint32_t restore_failure{};
    a::Event after_wait{a::Focus::Foreground};
    a::Event initial{a::Focus::Foreground};std::deque<a::Event> queue;
    void Start()override{if(start_fail)throw std::runtime_error("start fixture");assert(!started);started=true;++starts;}
    void Stop()noexcept override{assert(started);started=false;++stops;}
    a::Event Read()override{if(read_fail)throw std::runtime_error("read fixture");return initial;}
    bool Next(a::Event& event)override{assert(started);if(pump_fail)throw std::runtime_error("pump fixture");if(queue.empty())return false;event=queue.front();queue.pop_front();++reads;return true;}
    void SetVisible(bool value)override{assert(started);++shows;if(show_fail)throw std::runtime_error("show fixture");visible=value;}
    void Wait()override{assert(started);++waits;queue.push_back(after_wait);}
    std::uint32_t RestoreVisible(bool value)noexcept override{assert(!started);++restores;if(!restore_failure)visible=value;return restore_failure;}
};
template<class F>void Fails(F operation){bool threw=false;try{operation();}catch(const std::exception&){threw=true;}assert(threw);}
int main(){
    CForm::StaticInit();Form form;g_FormCur=&form;
    srhd_awa::matrixgame::platform3g::AppletEngineEvents events;
    const auto unrelated=std::uint32_t(GFLAG_FULLSCREEN|GFLAG_PRESENT_REQUIRED);
    g_Flags=unrelated;Backend backend;a::Lifecycle lifecycle(backend,events,false);
    Fails([&]{lifecycle.Show(true);});Fails([&]{lifecycle.Update();});
    lifecycle.Attach();assert((g_Flags&GFLAG_APPACTIVE) && !lifecycle.Drawable());
    assert(form.events==std::vector<ESysEvent>{SYSEV_ACTIVATED});
    Fails([&]{lifecycle.Attach();});assert(backend.starts==1);
    lifecycle.Show(true);assert(backend.visible && lifecycle.Drawable());
    lifecycle.Show(true);assert(backend.shows==1);
    backend.queue={{a::Focus::Unfocused},{a::Focus::Background},{a::Focus::Foreground}};
    lifecycle.Update();assert(form.events==std::vector<ESysEvent>({SYSEV_ACTIVATED,SYSEV_DEACTIVATING,SYSEV_ACTIVATED}));
    assert((g_Flags&unrelated)==unrelated);
    backend.queue={{a::Focus::Background}};lifecycle.Update();assert(!(g_Flags&GFLAG_APPACTIVE) && !lifecycle.Drawable());
    backend.queue={{a::Focus::Foreground}};lifecycle.Update();assert(lifecycle.Drawable());
    g_Flags|=GFLAG_KEEPALIVE;const auto count=form.events.size();
    backend.queue={{a::Focus::Background}};lifecycle.Update();
    assert((g_Flags&GFLAG_APPACTIVE) && !lifecycle.Drawable() && form.events.size()==count);
    backend.queue={{a::Focus::Foreground}};lifecycle.Update();g_Flags&=~GFLAG_KEEPALIVE;
    lifecycle.Show(false);assert(!backend.visible && !lifecycle.Drawable());
    Fails([&]{lifecycle.WaitForDrawable();});assert(backend.waits==0);
    backend.show_fail=true;Fails([&]{lifecycle.Show(true);});assert(!lifecycle.Drawable() && !backend.visible);
    backend.show_fail=false;lifecycle.Show(true);
    backend.queue={{a::Focus::Background}};lifecycle.WaitForDrawable();assert(backend.waits==1 && lifecycle.Drawable());
    backend.pump_fail=true;Fails([&]{lifecycle.Update();});backend.pump_fail=false;
    for(unsigned i=0;i<300;++i)backend.queue.push_back({i%2?a::Focus::Foreground:a::Focus::Background});
    const auto before=backend.reads;lifecycle.Update();assert(backend.reads-before==256 && backend.queue.size()==44);
    lifecycle.Update();assert(backend.queue.empty() && lifecycle.Drawable());
    backend.queue={{a::Focus::Background}};backend.after_wait={a::Focus::Background,true};
    try{lifecycle.WaitForDrawable();assert(false);}catch(const a::ExitRequested&){}
    assert(backend.waits==2);
    assert(g_Flags&GFLAG_APPCLOSE);assert(!lifecycle.Drawable());
    Fails([&]{lifecycle.Update();});Fails([&]{lifecycle.Show(true);});
    assert(lifecycle.Detach()==0 && !backend.visible && !backend.started && backend.restores==1);
    lifecycle.Detach();assert(backend.stops==1);
    g_FormCur=nullptr;lifecycle.Attach();assert(!lifecycle.Drawable());lifecycle.Show(true);assert(lifecycle.Drawable());lifecycle.Detach();
    // A failed noexcept restore remains observable, and blocks unsafe reattach.
    Backend broken;a::Lifecycle bad(broken,events,false);bad.Attach();bad.Show(true);broken.restore_failure=0x1234;
    assert(bad.Detach()==0x1234 && bad.RestoreResult()==0x1234 && !broken.started);Fails([&]{bad.Attach();});
    Backend initial_visible;initial_visible.visible=true;a::Lifecycle prior(initial_visible,events,true);
    prior.Attach();prior.Show(false);prior.Detach();assert(initial_visible.visible && initial_visible.restores==1);
    Backend failure;a::Lifecycle rollback(failure,events,false);failure.start_fail=true;
    Fails([&]{rollback.Attach();});assert(!failure.started && failure.stops==0);
    failure.start_fail=false;failure.read_fail=true;Fails([&]{rollback.Attach();});assert(!failure.started && failure.stops==1);
    failure.read_fail=false;g_FormCur=&form;form.fail=true;Fails([&]{rollback.Attach();});assert(!failure.started && failure.stops==2);form.fail=false;
    g_FormCur=nullptr;
    std::puts("native applet lifecycle CPU PASS focus/resume/background KEEPALIVE exit visibility restore failures bounded_pump original_CForm IPC_MODEL_ONLY GPU_NOT_RUN");
}
