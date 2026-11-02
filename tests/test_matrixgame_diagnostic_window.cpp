// CPU ownership/IPC model only: real Lifecycle and InitAsNative/Deinit execute.
// No libnx IPC, GPU rendering, full CGame::Init or map load is claimed here.
#include "native_diagnostic_window.hpp"
#include "applet_window_lifecycle.hpp"
#include <cassert>
#include <cstdio>
#include <string>
namespace mg=srhd_awa::matrixgame;
namespace a=mg::applet_window;
namespace p=mg::platform3g;
template<class F>void Reject(F f,const char* message=nullptr){
    bool rejected=false;try{f();}catch(const std::exception& e){rejected=true;if(message)assert(std::string(e.what())==message);}assert(rejected);
}
struct Backend final:a::Backend {
    unsigned starts{},stops{},active{},peak{},restores{};bool fail_read{},fail_start{},visible{true};std::uint32_t restore_failure{};
    void Start()override{if(fail_start)throw std::runtime_error("start failure");assert(!active);++starts;++active;if(active>peak)peak=active;}
    void Stop()noexcept override{assert(active==1);--active;++stops;}
    a::Event Read()override{if(fail_read)throw std::runtime_error("read failure");return {a::Focus::Foreground};}
    bool Next(a::Event&)override{assert(active==1);return false;}
    void Wait()override{assert(false);}
    void SetVisible(bool v)override{visible=v;}
    std::uint32_t RestoreVisible(bool v)noexcept override{++restores;if(!restore_failure)visible=v;return restore_failure;}
};
struct Events final:a::Observer {void FocusChanged(bool)override{}void Exit()noexcept override{}};
struct Window final:p::Window {
    Backend backend;Events events;a::Lifecycle lifecycle{backend,events,true};
    struct Lease final:p::WindowAttachment {
        a::Lifecycle& life;bool owned{true};
        explicit Lease(a::Lifecycle& v):life(v){life.Attach();}
        ~Lease(){Restore();}
        void Restore()noexcept override{if(owned){life.Detach();owned=false;}}
    };
    void* Identity()const noexcept override{return const_cast<Window*>(this);}
    std::unique_ptr<p::WindowAttachment> AttachNative()override{return std::make_unique<Lease>(lifecycle);}
    std::unique_ptr<p::WindowAttachment> Attach(p::MessageHandler)override{throw std::logic_error("Win32 unexpected");}
    void Show(p::Visibility v)override{lifecycle.Show(v!=p::Visibility::Minimized);}
    void Update()override{lifecycle.Update();}
    void ReleaseCursor()override{throw std::logic_error("Win32 cursor unexpected");}
    std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)override{throw std::logic_error("Win32 message unexpected");}
};
struct Device final:p::Device {
    bool fail_caps{};unsigned caps{};
    p::DeviceCaps ReadCaps()override{++caps;if(fail_caps)throw std::runtime_error("caps failure");return {};}
    std::uint64_t AvailableTextureBytes()override{return 64*1024*1024;}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw std::logic_error("gamma unexpected");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{assert(false);}
};
void Drawable(Window& w){w.lifecycle.WaitForDrawable();assert(w.lifecycle.Drawable());}
void Balanced(Window& w){assert(!w.backend.active && !w.lifecycle.Attached());assert(w.backend.starts==w.backend.stops && w.backend.peak==1);assert(!p::Current().ready && !p::Current().attachment);}
int main(){
    unsigned checkpoint_calls{};Window w;Device device;
    // Exact first hardware failure reproduced with the production lifecycle.
    Reject([&]{Drawable(w);},"native applet window not attached");
    w.lifecycle.SetWaitCheckpoint([&]{++checkpoint_calls;});
    {
        mg::diagnostic::WindowPhase phase(w);phase.Acquire();Drawable(w);
        Reject([&]{phase.Acquire();});assert(w.backend.starts==1);
        Reject([&]{w.AttachNative();},"native applet window already attached");assert(w.backend.starts==1);
        phase.Release();phase.Release();assert(w.backend.stops==1);
        // Actual production original-platform entry acquires its own lease.
        p::InitAsNative(w,device);assert(p::Current().ready && device.caps==1);Drawable(w);
        Reject([&]{phase.Acquire();});Drawable(w);
        p::Deinit();Balanced(w);
        phase.Acquire();Drawable(w);phase.Release();Balanced(w);
    }
    assert(checkpoint_calls==4); // Same policy survives early -> original -> error owners.
    // Smoke failure: early owner remains available for the real error-UI path.
    try{mg::diagnostic::WindowPhase phase(w);phase.Acquire();try{throw std::runtime_error("smoke failure");}catch(...){assert(phase.Owned());Drawable(w);throw;}}
    catch(const std::runtime_error&){}Balanced(w);
    // Original platform negotiation failure unwinds its attachment, then UI reattaches.
    {
        mg::diagnostic::WindowPhase phase(w);phase.Acquire();phase.Release();device.fail_caps=true;
        Reject([&]{p::InitAsNative(w,device);},"caps failure");Balanced(w);device.fail_caps=false;
        phase.Acquire();Drawable(w);
    }Balanced(w);
    // Fail after original platform success: cleanup precedes error owner.
    {
        mg::diagnostic::WindowPhase phase(w);phase.Acquire();phase.Release();p::InitAsNative(w,device);
        try{throw std::runtime_error("later Init failure");}catch(...){p::Deinit();phase.Acquire();Drawable(w);}
    }Balanced(w);
    w.backend.fail_start=true;{mg::diagnostic::WindowPhase phase(w);Reject([&]{phase.Acquire();});assert(!phase.Owned());}Balanced(w);w.backend.fail_start=false;
    w.backend.fail_read=true;{mg::diagnostic::WindowPhase phase(w);Reject([&]{phase.Acquire();});assert(!phase.Owned());}Balanced(w);w.backend.fail_read=false;
    // Application exit/exception before transfer releases exactly once.
    try{mg::diagnostic::WindowPhase phase(w);phase.Acquire();throw a::ExitRequested();}catch(const a::ExitRequested&){}Balanced(w);
    // A failed visibility restore blocks handoff rather than hiding broken state.
    Window broken;{mg::diagnostic::WindowPhase phase(broken);phase.Acquire();broken.lifecycle.Show(false);broken.backend.restore_failure=0x1234;phase.Release();assert(broken.lifecycle.RestoreResult()==0x1234);Reject([&]{p::InitAsNative(broken,device);},"previous applet visibility restore failed");}Balanced(broken);
    std::puts("WINDOW_HANDOFF PASS baseline_not_attached reproduced original_InitAsNative executed exclusive_peak=1 smoke_error init_error cancel start_read_failure restore_failure reentry CPU_IPC_MODEL_ONLY NO_GPU NO_FULL_CGAME_INIT");
}
