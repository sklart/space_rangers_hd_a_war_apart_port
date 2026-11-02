// Real Lifecycle + RunControl, deterministic IPC/clock model. No HOS/GPU.
#include "applet_window_lifecycle.hpp"
#include "native_diagnostic_control.hpp"
#include <cassert>
#include <cstdio>
#include <string>
namespace a=srhd_awa::matrixgame::applet_window;
namespace d=srhd_awa::matrixgame::diagnostic;
struct Cancelled:std::exception{};
struct Deadline:std::exception{};
struct Backend final:a::Backend {
    bool live{},foreground{},held{},pending{},exit{},fail_wait{};unsigned waits{},starts{},stops{},resume_at{3},press_at{},exit_at{};std::uint64_t now{};
    void Start()override{assert(!live);live=true;++starts;}
    void Stop()noexcept override{assert(live);live=false;++stops;}
    a::Event Read()override{return {foreground?a::Focus::Foreground:a::Focus::Background,exit};}
    bool Next(a::Event& event)override{assert(live);if(!pending)return false;pending=false;event=Read();return true;}
    void Wait()override{assert(live);++waits;now+=50'000;if(fail_wait)throw std::runtime_error("wait fixture failure");if(waits==resume_at){foreground=true;pending=true;}if(waits==press_at)held=true;if(waits==exit_at){exit=true;pending=true;}}
    void SetVisible(bool)override{}
    std::uint32_t RestoreVisible(bool)noexcept override{return 0;}
};
struct Observer final:a::Observer {unsigned changes{},exits{};void FocusChanged(bool)override{++changes;}void Exit()noexcept override{++exits;}};
struct Owner {a::Lifecycle& life;explicit Owner(a::Lifecycle& l):life(l){life.Attach();}~Owner(){life.Detach();}};
int main(int argc,char**){
    const bool baseline=argc>1;unsigned cases{};
    {
        Backend backend;Observer observer;d::RunControl control(0,120'000,4);a::Lifecycle life(backend,observer,true);unsigned polls{};
        life.SetWaitCheckpoint([&]{++polls;if(control.Poll(backend.now,backend.held)==d::RunControl::PollResult::Deadline)throw Deadline();});
        bool deadline{},draw_started{};{Owner owner(life);try{life.WaitForDrawable();draw_started=true;}catch(const Deadline&){deadline=true;}}
        assert(backend.waits==3 && backend.starts==backend.stops);assert(deadline==!baseline && draw_started==baseline);assert(polls==(baseline?0u:4u));++cases;
    }
    {
        Backend backend;backend.press_at=1;Observer observer;d::RunControl control(0,1'000'000,4);a::Lifecycle life(backend,observer,true);
        life.SetWaitCheckpoint([&]{if(control.Poll(backend.now,backend.held)==d::RunControl::PollResult::UserExit)throw Cancelled();});
        bool cancelled{},draw_started{};{Owner owner(life);try{life.WaitForDrawable();draw_started=true;}catch(const Cancelled&){cancelled=true;}}
        assert(cancelled==!baseline && draw_started==baseline && backend.waits==(baseline?3u:1u));assert(!backend.live && backend.starts==backend.stops);++cases;
    }
    {
        Backend backend;backend.foreground=true;backend.now=200'000;Observer observer;d::RunControl control(0,120'000,4);a::Lifecycle life(backend,observer,true);
        life.SetWaitCheckpoint([&]{if(control.Poll(backend.now,false)==d::RunControl::PollResult::Deadline)throw Deadline();});
        bool deadline{};{Owner owner(life);try{life.WaitForDrawable();}catch(const Deadline&){deadline=true;}}assert(deadline==!baseline && backend.waits==0);++cases;
    }
    {
        Backend backend;backend.exit_at=1;Observer observer;a::Lifecycle life(backend,observer,true);unsigned polls{};
        life.SetWaitCheckpoint([&]{++polls;});bool exited{};{Owner owner(life);try{life.WaitForDrawable();}catch(const a::ExitRequested&){exited=true;}}
        assert(exited && observer.exits==1 && polls==(baseline?0u:1u));assert(!backend.live);++cases;
    }
    {
        Backend backend;backend.resume_at=2;Observer observer;a::Lifecycle life(backend,observer,true);
        {Owner owner(life);life.WaitForDrawable();assert(life.Drawable() && backend.waits==2);}assert(backend.starts==backend.stops);++cases;
    }
    {
        Backend backend;backend.foreground=true;Observer observer;a::Lifecycle life(backend,observer,true);unsigned polls{};
        life.SetWaitCheckpoint([&]{++polls;});{Owner owner(life);life.WaitForDrawable();}{Owner owner(life);life.WaitForDrawable();}
        life.SetWaitCheckpoint({});{Owner owner(life);life.WaitForDrawable();}assert(polls==(baseline?0u:2u) && backend.starts==3 && backend.stops==3);++cases;
    }
    {
        Backend backend;backend.fail_wait=true;Observer observer;a::Lifecycle life(backend,observer,true);unsigned polls{};
        life.SetWaitCheckpoint([&]{++polls;});bool failed{};{Owner owner(life);try{life.WaitForDrawable();}catch(const std::runtime_error& e){failed=std::string(e.what())=="wait fixture failure";}}
        assert(failed && backend.starts==backend.stops && polls==(baseline?0u:1u));++cases;
    }
    std::printf("DRAWABLE_CHECKPOINT %s cases=%u actual_Lifecycle actual_RunControl deadline user_cancel foreground_fast_path HOS_exit_precedence no_hook reattach clear_hook wait_error CPU_CLOCK_IPC_MODEL_ONLY NO_GPU\n",baseline?"BASELINE_DEADLINE_AND_CANCEL_IGNORED":"PASS",cases);
}
