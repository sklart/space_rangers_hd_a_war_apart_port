// Exact production AcquirePrimary bodies; real Lifecycle/FrameScope/StateStore.
// Window IPC and queue.acquireImage are explicit CPU endpoints, not HOS/GPU.
#include "applet_window_lifecycle.hpp"
#include "native_frame_scope.hpp"
#include "native_draw_batch.hpp"
#include "native_device_state.hpp"
#include <cassert>
#include <cstdio>
#include <deque>
#include <functional>
#include <string>
namespace mg=srhd_awa::matrixgame;
namespace a=mg::applet_window;
namespace p=mg::platform3g;
using p::Unsupported;
struct Backend final:a::Backend {
    bool live{};unsigned polls{},waits{};a::Focus focus{a::Focus::Foreground};std::deque<a::Event> events;
    void Start()override{assert(!live);live=true;}
    void Stop()noexcept override{assert(live);live=false;}
    a::Event Read()override{return {focus,false};}
    bool Next(a::Event& event)override{assert(live);++polls;if(events.empty())return false;event=events.front();events.pop_front();focus=event.focus;return true;}
    void Wait()override{++waits;events.push_back({a::Focus::Foreground,false});}
    void SetVisible(bool)override{}
    std::uint32_t RestoreVisible(bool)noexcept override{return 0;}
};
struct Observer final:a::Observer {
    unsigned losses{},exits{};std::function<void()> readback;
    void FocusChanged(bool foreground)override{
        if(!foreground){++losses;readback();}
    }
    void Exit()noexcept override{++exits;}
};
struct Harness {
    p::NativeStateStore state;Backend backend;Observer observer;a::Lifecycle life{backend,observer,true};
    struct Window {a::Lifecycle& life;unsigned waits{};p::WindowRect size{0,0,1280,720};
        void WaitForDrawable(){++waits;life.WaitForDrawable();}p::WindowRect ReadClientRect(){return size;}
    } window{life};
    struct Presentation {bool ready{true};bool Ready()const{return ready;}} presentation_state;
    struct Queue {int result{1};unsigned calls{};int acquireImage(int swapchain){assert(swapchain==73);++calls;return result;}} queue;
    struct Target {int swapchain{73};struct {unsigned BackBufferCount{2};}value;} target;
    Target& Targets(){return target;}
    p::WindowRect extent{0,0,1280,720};int image_slot{-1};p::NativeFrameScope frame;
    bool recording{};unsigned submits{},retirements{};p::NativeDrawBatch batch;
    void SubmitAndWait(){++submits;}
    void ReleaseCompleted()noexcept{++retirements;recording=false;}
    void DrainBatch(){p::DrainNativeDrawBatch(frame,batch,*this);}
    Harness(){observer.readback=[&]{FlushRecording();};life.Attach();}
    ~Harness(){Finish();life.Detach();}
    // Same admission order as production StartRecording, without queue/memory work.
    void Admit(){window.WaitForDrawable();frame.Acquire();state.Frame(true);recording=true;}
    void Finish(){frame.Abort();state.Frame(false);image_slot=-1;recording=false;batch.Retired();}
#include "flush-recording.inc"
#include "baseline-acquire.inc"
#include "candidate-acquire.inc"
};
template<class F>std::string Failure(F fn){try{fn();}catch(const std::exception& e){return e.what();}return {};}
int main(){
    unsigned cases{},baseline_aborts{},baseline_scene_conflicts{},deferred_readbacks{};
    for(bool baseline:{true,false}){
        const auto acquire=baseline?&Harness::Baseline:&Harness::Candidate;
        {
            Harness h;h.Admit();(h.*acquire)();const auto polls=h.backend.polls;
            (h.*acquire)();assert(h.image_slot==1 && h.queue.calls==1 && h.backend.polls==polls);++cases;
        }
        {
            Harness h;h.Admit();h.backend.events.push_back({a::Focus::Background,false});
            const auto failure=Failure([&]{(h.*acquire)();});
            if(baseline){assert(failure.empty() && !h.frame && !h.recording && h.queue.calls==1 && h.observer.losses==1 && h.submits==1 && h.retirements==1);++baseline_aborts;}
            else{
                assert(failure.empty() && h.queue.calls==1 && h.observer.losses==0 && h.backend.events.size()==1);
                assert(h.frame && h.recording && !h.submits);h.Finish();h.Admit();assert(h.observer.losses==1 && h.backend.waits==1 && h.life.Drawable() && h.frame && !h.submits);
                ++deferred_readbacks;
            }++cases;
        }
        {
            Harness h;h.Admit();h.frame.BeginScene();h.backend.events.push_back({a::Focus::Background,false});
            const auto failure=Failure([&]{(h.*acquire)();});
            if(baseline){assert(failure=="native Present requires acquired frame with closed scene" && !h.queue.calls && h.observer.losses==1);++baseline_scene_conflicts;}
            else{assert(failure.empty() && h.frame && h.queue.calls==1 && h.observer.losses==0);}
            ++cases;
        }
        {
            Harness h;h.Admit();h.backend.events.push_back({a::Focus::Foreground,true});
            const auto failure=Failure([&]{(h.*acquire)();});
            if(baseline){assert(failure=="native applet exit requested" && h.observer.exits==1 && !h.queue.calls);}
            else{assert(failure.empty() && !h.observer.exits && h.queue.calls==1);h.Finish();assert(Failure([&]{h.Admit();})=="native applet exit requested" && h.observer.exits==1 && !h.frame);}
            ++cases;
        }
        {
            Harness h;h.backend.events.push_back({a::Focus::Background,false});h.Admit();assert(h.observer.losses==1 && h.backend.waits==1 && !h.submits);(h.*acquire)();assert(h.image_slot==1);++cases;
        }
        {
            Harness h;h.Admit();h.presentation_state.ready=false;
            assert(Failure([&]{(h.*acquire)();})=="native presentation requires successful Reset" && !h.queue.calls);++cases;
        }
        {
            Harness h;h.Admit();h.window.size.right=640;
            assert(Failure([&]{(h.*acquire)();})=="native drawable extent changed; recreation required" && !h.queue.calls);++cases;
        }
        for(int slot:{-1,2}){
            Harness h;h.Admit();h.queue.result=slot;assert(Failure([&]{(h.*acquire)();})=="native swapchain acquire failed" && h.queue.calls==1);++cases;
        }
        {
            Harness h;h.Admit();h.frame.BeginScene();(h.*acquire)();assert(h.image_slot==1);h.frame.EndScene();++cases;
        }
    }
    for(bool submitted:{false,true}){
        Harness h;if(submitted){h.Admit();h.frame.Submitted();}
        assert(Failure([&]{h.Candidate();})=="native batch requires acquired image before presentation submission" && !h.queue.calls);++cases;
    }
    assert(baseline_aborts==1 && baseline_scene_conflicts==1 && deferred_readbacks==1);
    std::printf("FRAME_FOCUS PASS cases=%u baseline_scope_aborts=%u baseline_scene_conflicts=%u deferred_readbacks=%u exact_AcquirePrimary_FlushRecording actual_Lifecycle_FrameScope_StateStore CPU_ENDPOINTS NO_GPU_NO_ORIGINAL_INIT\n",cases,baseline_aborts,baseline_scene_conflicts,deferred_readbacks);
}
