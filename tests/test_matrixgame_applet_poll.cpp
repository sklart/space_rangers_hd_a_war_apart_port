// Real pinned libnx appletGetMessage body and production poll/lifecycle;
// eventWait/IPC endpoints are deterministic fixtures. No actual HOS/GPU.
#include "native_applet_message.hpp"
#include "applet_window_lifecycle.hpp"
#include <switch/result.h>
#include <cassert>
#include <cstdio>
#include <deque>
namespace a=srhd_awa::matrixgame::applet_window;
namespace p=srhd_awa::matrixgame::platform3g;
struct Fixture {Result wait{KERNELRESULT(TimedOut)},receive{};u32 message{0xbad};};
Fixture fixture;
unsigned waits{},receives{},processes{};
struct EventFixture{}g_appletMessageEvent;
Result eventWait(EventFixture*,u64 timeout){assert(timeout==0);++waits;return fixture.wait;}
Result _appletReceiveMessage(u32* message){++receives;*message=fixture.message;return fixture.receive;}
[[noreturn]] void diagAbortWithResult(Result result){assert(result==MAKERESULT(Module_Libnx,LibnxError_BadAppletReceiveMessage));throw std::runtime_error("SDK receive fatal fixture");}
#include "original-applet-get-message.inc"
auto Poll(){return a::PollMessage(appletGetMessage,[](u32 msg){++processes;assert(msg==42);return true;});}
struct Backend final:a::Backend {
    std::deque<Fixture> script;bool live{};a::Focus focus{a::Focus::Foreground};bool exit{};unsigned starts{},stops{},sleep{};
    void Start()override{assert(!live);live=true;++starts;}
    void Stop()noexcept override{assert(live);live=false;++stops;}
    a::Event Read()override{return {focus,exit};}
    bool Next(a::Event& event)override{
        assert(live);fixture=script.empty()?Fixture{}:script.front();if(!script.empty())script.pop_front();
        auto processed=a::PollMessage(appletGetMessage,[&](u32 msg){++processes;if(msg==1)focus=a::Focus::Background;if(msg==2)focus=a::Focus::Foreground;if(msg==3)exit=true;return !exit;});
        if(!processed)return false;event=Read();return true;
    }
    void Wait()override{assert(live);++sleep;script.push_back({0,0,2});}
    void SetVisible(bool)override{assert(live);}
    std::uint32_t RestoreVisible(bool)noexcept override{return 0;}
};
struct Observer final:a::Observer {unsigned changes{},exits{};void FocusChanged(bool)override{++changes;}void Exit()noexcept override{++exits;}};
int main(){
    static_assert(a::MessageTimedOut==KERNELRESULT(TimedOut));
    static_assert(a::MessageQueueEmpty==MAKERESULT(128,3));
    // Run2 baseline: actual libnx body returns timeout before receiving IPC.
    u32 message=0xfeed;fixture={};const auto result=appletGetMessage(&message);
    assert(result==59905 && message==0xfeed && receives==0);
    bool baseline_failed=false;try{if(result!=MAKERESULT(128,3))p::CheckDeviceResult(result,"appletGetMessage");}catch(const p::DeviceError& e){baseline_failed=true;assert(e.result==59905);}assert(baseline_failed);
    assert(!Poll());assert(receives==0 && processes==0);
    fixture={0,MAKERESULT(128,3),42};assert(!Poll());assert(receives==1 && processes==0);
    fixture={0,0,42};assert(Poll()==true);assert(processes==1);
    assert(a::MessageUnavailable(KERNELRESULT(TimedOut)|0x80000000u));
    assert(a::MessageUnavailable(MAKERESULT(128,3)|0x80000000u));
    // Every other result remains fatal to this adapter; do not hide real errors.
    for(Result error:{KERNELRESULT(InvalidHandle),KERNELRESULT(Cancelled),KERNELRESULT(OutOfMemory),MAKERESULT(Module_Libnx,LibnxError_NotInitialized)}){
        fixture={error,0,42};const auto before=processes;bool threw=false;
        try{Poll();}catch(const p::DeviceError& e){threw=true;assert(std::uint32_t(e.result)==error);}assert(threw && processes==before);
    }
    fixture={0,KERNELRESULT(InvalidHandle),42};bool fatal=false;try{Poll();}catch(const std::runtime_error& e){fatal=std::string(e.what())=="SDK receive fatal fixture";}assert(fatal);
    assert(appletGetMessage(nullptr)==MAKERESULT(Module_Libnx,LibnxError_BadInput));
    Backend backend;Observer observer;a::Lifecycle life(backend,observer,true);
    life.Attach();for(unsigned i=0;i<1000;++i)life.Update();life.WaitForDrawable();assert(life.Drawable() && backend.sleep==0);
    backend.script.push_back({0,0,1});life.Update();assert(!life.Drawable());
    life.WaitForDrawable();assert(life.Drawable() && backend.sleep==1);
    backend.script.push_back({0,0,3});bool exited=false;try{life.Update();}catch(const a::ExitRequested&){exited=true;}assert(exited && observer.exits==1);
    life.Detach();assert(backend.starts==1 && backend.stops==1 && !backend.live);
    std::puts("APPLET_POLL PASS run2_59905_baseline_reproduced original_libnx_GetMessage executed empty_timeout AM_race message focus_resume exit real_errors_preserved 1000_idle_updates CPU_IPC_MODEL_ONLY NO_GPU");
}
