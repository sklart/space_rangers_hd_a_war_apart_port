// Real FrameScope/StateStore and extracted production completion/flush bodies.
// Queue and native-window endpoints below are CPU models, not Switch execution.
#include "native_device_state.hpp"
#include "native_draw_batch.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
using p::Unsupported;
namespace diagnostic {struct Operation {explicit Operation(const char*){}};}
struct NvMultiFence {};
struct Window {unsigned result{},cancels{};std::vector<std::string>* events{};int native{7};int& NativeWindow(){return native;}};
Window* current_window{};
unsigned nwindowCancelBuffer(int* window,int slot,NvMultiFence*){
    assert(window && *window==7 && slot==1);++current_window->cancels;current_window->events->push_back("cancel");return current_window->result;
}
struct Harness {
    p::NativeFrameScope frame;p::NativeStateStore state;p::NativeDrawBatch batch;
    bool recording{},healthy{true},fail_wait{},primary_readback{},presentation_readback{};
    int image_slot{-1};unsigned owners{},submits{},retirements{},presents{};
    std::vector<std::string> events;Window window;Harness* impl_{this};
    Harness(){window.events=&events;current_window=&window;state.Edit().cull=2;}
    bool Healthy(){return healthy;}
    void Admit(bool image=false){frame.Acquire();state.Frame(true);recording=true;image_slot=image?1:-1;owners=3;batch.Add(4096);}
    void SubmitAndWait(){events.push_back("submit_wait");++submits;if(fail_wait)throw Unsupported("CPU completion failure");}
    void ReleaseCompleted()noexcept{events.push_back("release");++retirements;recording=false;owners=0;}
    void DrainBatch(){p::DrainNativeDrawBatch(frame,batch,*this);}
#include "flush-recording.inc"
#include "complete-init.inc"
};
template<class F>std::string Failure(F f){try{f();}catch(const std::exception& e){return e.what();}return {};}
void Ready(Harness& h){
    assert(!h.frame && !h.recording && h.image_slot==-1 && !h.owners && !h.batch.Draws() && !h.presents);
    auto state=h.state.Capture();assert(state && h.state.Read().cull==2);h.state.Edit().cull=3;assert(state->Restore()==0 && h.state.Read().cull==2);state.reset();
    // Next actual form can acquire/open/draw/close rather than reuse a stale Init frame.
    h.frame.Acquire();h.state.Frame(true);h.frame.BeginScene();h.frame.RequireDraw();h.frame.EndScene();h.frame.RequirePresent();
}
int main(){
    {Harness h;h.Admit();h.frame.BeginScene();h.frame.EndScene();
     assert(Failure([&]{h.state.Capture();})=="native state capture requires a quiescent frame");
     h.CompleteNativeInitializationWork();assert(h.submits==1 && h.retirements==1 && h.window.cancels==0);
     assert((h.events==std::vector<std::string>{"submit_wait","release"}));Ready(h);}
    {Harness h;h.Admit(true);h.CompleteNativeInitializationWork();assert(h.window.cancels==1);
     assert((h.events==std::vector<std::string>{"submit_wait","release","cancel"}));Ready(h);}
    {Harness h;h.CompleteNativeInitializationWork();assert(!h.submits && !h.window.cancels);Ready(h);}
    {Harness h;h.Admit();h.frame.BeginScene();assert(!Failure([&]{h.CompleteNativeInitializationWork();}).empty());assert(!h.submits && h.owners==3 && h.frame);}
    {Harness h;h.Admit(true);h.fail_wait=true;assert(!Failure([&]{h.CompleteNativeInitializationWork();}).empty());assert(h.owners==3 && h.frame && h.image_slot==1 && !h.retirements && !h.window.cancels);}
    {Harness h;h.Admit(true);h.window.result=123;assert(!Failure([&]{h.CompleteNativeInitializationWork();}).empty());assert(!h.owners && h.frame && h.image_slot==1 && h.retirements==1);assert(!Failure([&]{h.state.Capture();}).empty());}
    {Harness h;h.Admit();h.healthy=false;assert(!Failure([&]{h.CompleteNativeInitializationWork();}).empty());assert(h.owners==3 && h.frame && !h.submits);}
    for(bool primary:{true,false}){Harness h;h.Admit();(primary?h.primary_readback:h.presentation_readback)=true;assert(!Failure([&]{h.CompleteNativeInitializationWork();}).empty());assert(h.owners==3 && !h.submits && h.frame);}
    std::puts("INIT_HANDOFF PASS r6_capture_failure_reproduced production_flush_complete submit_wait_before_release_before_cancel state_preserved next_scene_ready open_scene_lost_queue_pending_readback_rejected failure_retains_required_owners no_present CPU_MODEL_ONLY NO_SWITCH_GPU");
}
