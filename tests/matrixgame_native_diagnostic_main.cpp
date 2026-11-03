// Diagnostic ELF entry. NRO packaging and hardware execution are separate gates.
#include "platform_switch.hpp"
#include "init_runtime.hpp"
#include "applet_window_lifecycle.hpp"
#include "MatrixMap.hpp"
#include "MatrixFormGame.hpp"
#include "native_loop_switch.hpp"
#include "native_diagnostic_journal.hpp"
#include "native_diagnostic_control.hpp"
#include "native_diagnostic_session.hpp"
#include "native_diagnostic_smoke.hpp"
#include "native_diagnostic_window.hpp"
#include "native-diagnostic-build.hpp"
#include "CException.hpp"
#ifdef MATRIXGAME_DIAGNOSTIC_RANGERS_HOST
#include "rangers_host_session.hpp"
#include "ec_file_adapter.hpp"
#endif
#include <switch.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <codecvt>
#include <cwchar>
#include <exception>
#include <locale>
#include <stdexcept>
namespace mg=srhd_awa::matrixgame;
namespace dg=mg::diagnostic;
namespace p=mg::platform3g;
namespace {
#ifdef MATRIXGAME_DIAGNOSTIC_MANUAL
constexpr auto SessionMode=dg::SessionMode::Interactive;
#else
constexpr auto SessionMode=dg::SessionMode::Quick;
#endif
constexpr bool Interactive=SessionMode==dg::SessionMode::Interactive;
struct UserExit:std::exception {const char* what()const noexcept override{return "diagnostic user exit";}};
struct Timeout:std::runtime_error {Timeout():std::runtime_error("diagnostic 120-second cooperative deadline exceeded") {}};
void CheckLibnx(Result result,const char* operation){
    dg::Record("result",operation,{},result);
    // libnx Result failures are positive values; HRESULT's sign test does not apply.
    if(R_FAILED(result)){dg::Record("exception",operation,"libnx Result failure",result);throw std::runtime_error(operation);}
}
class Cancellation {
    PadState pad_{};
    static std::uint64_t Now(){return std::chrono::duration_cast<std::chrono::microseconds>(mg::native_loop::Clock::now().time_since_epoch()).count();}
    dg::SessionControl control_{SessionMode,Now()};
public:
    Cancellation(){padInitializeDefault(&pad_);}
    void Poll(){
        padUpdate(&pad_);const auto held=padGetButtons(&pad_);
        const auto result=control_.Poll(Now(),{bool(held&HidNpadButton_B),bool(held&HidNpadButton_Plus),bool(held&HidNpadButton_Minus)});
        if(result==dg::RunControl::PollResult::UserExit)throw UserExit();
        if(result==dg::RunControl::PollResult::Deadline)throw Timeout();
    }
    bool Presented(std::uint64_t before,std::uint64_t after,bool visible){return control_.Presented(before,after,visible);}
    std::uint64_t Frames()const noexcept{return control_.Frames();}
    void BeginOriginalLoop(){
        const auto result=control_.BeginOriginalLoop(Now());
        if(result==dg::RunControl::PollResult::UserExit)throw UserExit();
        if(result==dg::RunControl::PollResult::Deadline)throw Timeout();
    }
    bool CountersDue()const noexcept{return control_.CountersDue();}
    bool ValidLoopReturn(int state)const noexcept{return control_.ValidLoopReturn(state);}
};
void Memory(){
    u64 total{},used{};const auto a=svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
    const auto b=svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
    dg::Record("result","process_memory.limit",{},a);if(R_SUCCEEDED(a))dg::Metric("process_memory_limit_bytes",total);
    dg::Record("result","process_memory.used",{},b);if(R_SUCCEEDED(b))dg::Metric("process_memory_used_bytes",used);
}
void Counters(p::DekoDevice& device){
    const auto s=device.Diagnostics();dg::Metric("gpu_buffer_allocations",s.buffer_allocations);
    dg::Metric("block_pool_hits",s.block_pool_hits);dg::Metric("block_pool_misses",s.block_pool_misses);dg::Metric("block_pool_bytes",s.block_pool_bytes);
    dg::Metric("block_pool_larger_hits",s.block_pool_larger_hits);dg::Metric("upload_requested_bytes",s.upload_requested_bytes);dg::Metric("upload_reserved_bytes",s.upload_reserved_bytes);
    dg::Metric("texture_pressure_events",s.texture_pressure_events);
    const auto profile=[](const char* name,const p::ProfileTotals& t){char key[96];
        std::snprintf(key,sizeof(key),"profile.%s.calls",name);dg::Metric(key,t.calls);
        std::snprintf(key,sizeof(key),"profile.%s.us",name);dg::Metric(key,t.us);
        std::snprintf(key,sizeof(key),"profile.%s.max_us",name);dg::Metric(key,t.maximum_us);};
    profile("draw_cpu",s.draw_cpu);profile("vertex_decode",s.vertex_decode);profile("block_create",s.block_create);
    profile("residency",s.residency);profile("command_encode",s.command_encode);profile("batch_wait",s.batch_wait);
    const auto io=dg::CurrentIOStats();dg::Metric("profile.journal.calls",io.calls);dg::Metric("profile.journal.bytes",io.bytes);dg::Metric("profile.journal.us",io.us);
    dg::Metric("gpu_buffer_bytes_cumulative",s.buffer_bytes_total);dg::Metric("recorded_draw_calls",s.recorded_draws);
    dg::Metric("queue_submission_calls",s.queue_submissions);dg::Metric("completed_presents",s.presents);
    dg::Metric("native_texture_session_uploads",s.session_uploads);dg::Metric("texture_arena_capacity",s.texture_capacity);
    dg::Metric("batch_drains",s.batch_drains);dg::Metric("max_live_draw_packets",s.max_live_draw_packets);dg::Metric("max_live_draw_bytes",s.max_live_draw_bytes);
    dg::Metric("managed_texture_evictions",device.TextureSession()->managed_evictions);
    dg::Metric("texture_arena_free",s.texture_free);dg::Metric("queue_healthy",s.queue_healthy);Memory();
    dg::Metric("SDK_warnings_total",s.sdk_warnings);dg::Metric("SDK_warning_records_suppressed",s.sdk_warnings>32?s.sdk_warnings-32:0);
}
void ImageFingerprint(const char* argv0){
    dg::Operation operation("entry.self_NRO_sha256");
    FILE* file=argv0?std::fopen(argv0,"rb"):nullptr;
    if(!file){operation.Fail(errno);dg::Record("unavailable","image_sha256","argv[0] is not readable; use packaged manifest",errno);return;}
    struct Owner{FILE* f;~Owner(){std::fclose(f);}} owner{file};
    Sha256Context hash;sha256ContextCreate(&hash);std::array<unsigned char,65536> buffer{};std::uint64_t bytes{};
    for(;;){const auto n=std::fread(buffer.data(),1,buffer.size(),file);if(n){sha256ContextUpdate(&hash,buffer.data(),n);bytes+=n;}if(n<buffer.size()){if(std::ferror(file))throw std::runtime_error("read own NRO digest failed");break;}}
    std::array<unsigned char,32> digest{};sha256ContextGetHash(&hash,digest.data());std::array<char,65> hex{};
    for(unsigned i=0;i<32;++i)std::snprintf(hex.data()+i*2,3,"%02x",digest[i]);
    dg::Record("metadata","image_sha256",hex.data());dg::Metric("image_file_bytes",bytes);
}
class BoundedOriginalLoop final:public mg::native_loop::Backend {
    mg::native_loop::SwitchBackend actual_;p::DekoDevice& device_;Cancellation& cancel_;dg::Journal& journal_;bool capture_requested_{};std::uint64_t transition_presents_{};
public:
    BoundedOriginalLoop(p::SwitchWindow& w,p::DekoDevice& d,Cancellation& c,dg::Journal& j):actual_(w,d),device_(d),cancel_(c),journal_(j){cancel_.BeginOriginalLoop();}
    void Pump()override{cancel_.Poll();actual_.Pump();cancel_.Poll();}
    void Idle()override{actual_.Idle();}
    void Draw(CForm& form)override{
        cancel_.Poll();dg::Operation operation("C.original_form.Draw");const auto before=device_.Diagnostics().presents;
        const bool visible=(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY) && !(g_MatrixMap->m_Flags&MMFLAG_TRANSITION);
        if(visible && !capture_requested_){device_.RequestPrimaryReadback([output=&journal_](auto bytes,unsigned w,unsigned h){dg::SaveFrame(*output,"original-first",w,h,bytes);});capture_requested_=true;}
        actual_.Draw(form);
        const auto after=device_.Diagnostics().presents;const auto done=cancel_.Presented(before,after,visible);
        if(!visible && after>before){transition_presents_+=after-before;dg::Metric("original_loading_presents",transition_presents_);if(transition_presents_<=4 || transition_presents_%30==0)Counters(device_);}
        if(visible && after>before && cancel_.CountersDue()){dg::Metric("original_loop_frames_presented",cancel_.Frames());Counters(device_);}
        if(done)g_Flags|=GFLAG_EXITLOOP;
    }
    std::uint64_t Frames()const noexcept{return cancel_.Frames();}
    bool ValidLoopReturn(int state)const noexcept{return cancel_.ValidLoopReturn(state);}
};
void Run(const char* root_arg,const char* rangers_root_arg,dg::Journal& journal){
    const auto resource_root=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(root_arg);
    const auto utf8_output=(journal.Directory()/"output").generic_u8string();
    const auto output_root=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(reinterpret_cast<const char*>(utf8_output.data()),reinterpret_cast<const char*>(utf8_output.data()+utf8_output.size()));
    {
        dg::Operation operation("A.libnx_environment");
        const auto hos=hosversionGet();char environment[256];
        std::snprintf(environment,sizeof(environment),"HOS=%u.%u.%u libnx_package=%s deko3d_package=%s applet_type=%u",hos>>16,(hos>>8)&255,hos&255,matrixgame_diagnostic_build::LibnxPackage,matrixgame_diagnostic_build::DekoPackage,unsigned(appletGetAppletType()));
        dg::Record("environment","libnx",environment);Memory();
        dg::Record("environment","deko_library",matrixgame_diagnostic_build::DekoLibrary);
        if(appletGetAppletType()!=AppletType_Application && appletGetAppletType()!=AppletType_SystemApplication)
            throw p::Unsupported("diagnostic requires hbmenu application mode; applet memory is insufficient");
        if(!serviceIsActive(hidGetServiceSession()) || !hidGetSharedmemAddr())throw p::Unsupported("diagnostic requires live libnx HID");
        hidInitializeNpad();const HidNpadIdType ids[]{HidNpadIdType_No1,HidNpadIdType_Handheld};
        CheckLibnx(hidSetSupportedNpadIdType(ids,2),"hidSetSupportedNpadIdType");
        CheckLibnx(hidSetSupportedNpadStyleSet(HidNpadStyleSet_NpadFullCtrl),"hidSetSupportedNpadStyleSet");
        CheckLibnx(appletSetAppletWindowVisibility(true),"appletSetAppletWindowVisibility");
    }
    Cancellation cancel;cancel.Poll();
    dg::Operation display_setup("A.SwitchDisplay");p::SwitchDisplay display;
    dg::Record("begin","A.SwitchWindow");p::SwitchWindow window(display.NativeWindow(),true,&display);dg::Record("ok","A.SwitchWindow");
    // cancel and journal outlive window/Device/runtime, including unwind.
    window.SetWaitCheckpoint([&]{cancel.Poll();journal.RequireHealthy();});
    dg::WindowPhase diagnostic_window(window);
    {dg::Operation attachment("A.window_attachment");diagnostic_window.Acquire();}
    {
        dg::Operation device_setup("A.native_device_owner");p::DekoDevice device(window);
#ifdef MATRIXGAME_DIAGNOSTIC_RANGERS_HOST
        // Declared before runtime: its callbacks/data/workers outlive Deinit on
        // success, Init failure, cooperative cancellation and stack unwinding.
        std::unique_ptr<mg::RangersHostSession> rangers_host;
#else
        (void)rangers_root_arg;
#endif
        mg::NativeInitRuntime runtime;const wchar_t* phase=L"A: GPU smoke";
        const auto hold=[&](){const auto until=mg::native_loop::Clock::now()+std::chrono::seconds(2);while(mg::native_loop::Clock::now()<until){cancel.Poll();window.Update();svcSleepThread(16'000'000);}};
        try{
        dg::RunSmoke(device,journal);Counters(device);cancel.Poll();
        const wchar_t* init_screen[]{L"Stage B: original MatrixGame Init",Interactive?L"Minus + Plus: exit at the next safe checkpoint":L"B / +: exit at the next safe checkpoint",L"Logs: sdmc:/switch/matrixgame-diag/logs"};
        dg::RenderStatus(device,init_screen);phase=L"B: original Init";
        mg::NativeGameOptions options{window,device};options.seed=37;options.draw_sink=&device;
        options.output_root=output_root.c_str();options.resource_root=resource_root.c_str();cancel.Poll();
#ifdef MATRIXGAME_DIAGNOSTIC_RANGERS_HOST
        {dg::Operation host_init("B.RangersHostSession");
            const auto user_path=(journal.Directory()/"rangers-user").generic_string();
            srhd_awa::platform::ec_file::SetGameRoot(rangers_root_arg);
            srhd_awa::platform::ec_file::SetUserRoot(user_path);
            rangers_host=std::make_unique<mg::RangersHostSession>();
            options.services=&rangers_host->Table();cancel.Poll();
        }
#endif
        {dg::Operation transfer("B.release_diagnostic_window");diagnostic_window.Release();
            CheckLibnx(window.LastVisibilityRestoreResult(),"diagnostic window visibility restore before original Init");}
        runtime.InitializeNativeGame(options);journal.RequireHealthy();dg::Record("ok","B.complete_original_Init");Counters(device);cancel.Poll();
        device.CompleteNativeInitializationWork();journal.RequireHealthy();
        phase=L"C: original Draw";const wchar_t* draw_screen[]{Interactive?L"Stage C: manual MatrixGame check":L"Stage C: original MatrixGame Draw",Interactive?L"Up to 120 seconds; original controls are active":L"First map frame readback; four completed frames",Interactive?L"Minus + Plus: safe exit; Plus: game menu":L"B / +: safe exit"};dg::RenderStatus(device,draw_screen);
        {
            mg::native_form::Session form;
            BoundedOriginalLoop backend(window,device,cancel,journal);mg::native_loop::Scope loop(backend);
            g_ExitState=0;g_Flags&=~GFLAG_EXITLOOP;RunOriginalNativeLoop();
            dg::Metric("original_loop_frames_presented_final",backend.Frames());
            if(!backend.ValidLoopReturn(g_ExitState))throw std::runtime_error("original loop ended without valid diagnostic frames/exit state");
            dg::Metric("original_exit_state",g_ExitState);
        }
        {
            dg::Operation snapshot("C.original_SaveResult");SRobotGameState statistics{};
            runtime.SaveOriginalGameResult(statistics);
            // Preserve signed time (winning sides negate it) without converting
            // it to the journal's unsigned Metric field. These are snapshots,
            // including quick-mode early exit; no battle outcome is fabricated.
            dg::Record("statistics","original_time_signed_ms",{},statistics.m_Time);
            dg::Record("statistics","original_robots_built",{},statistics.m_BuildRobot);
            dg::Record("statistics","original_robots_destroyed",{},statistics.m_KillRobot);
            dg::Record("statistics","original_turrets_built",{},statistics.m_BuildTurret);
            dg::Record("statistics","original_turrets_destroyed",{},statistics.m_KillTurret);
            dg::Record("statistics","original_buildings_destroyed",{},statistics.m_KillBuilding);
            journal.RequireHealthy();
        }
        hold();phase=L"D: original cleanup";
        dg::Record("begin","D.original_runtime");runtime.Deinit();dg::Record("ok","D.original_runtime");
        const auto restored=runtime.LastStateRestoreResult();if(!restored)throw std::runtime_error("original runtime returned no device restore result");
        p::CheckDeviceResult(*restored,"diagnostic original device state restore");Counters(device);
#ifdef MATRIXGAME_DIAGNOSTIC_RANGERS_HOST
        {dg::Operation release("D.RangersHostSession");rangers_host.reset();}
#endif
        }catch(const UserExit&){dg::Metric("original_loop_frames_presented_final",cancel.Frames());throw;}
        catch(const mg::applet_window::ExitRequested&){dg::Metric("original_loop_frames_presented_final",cancel.Frames());throw;}
        catch(...){
            const auto failure=std::current_exception();
            const auto aborted=device.AbortNativeFrame();dg::Record("result","diagnostic.error_abort",{},aborted);Counters(device);runtime.Deinit();
#ifdef MATRIXGAME_DIAGNOSTIC_RANGERS_HOST
            {dg::Operation release("D.RangersHostSession.error");rangers_host.reset();}
#endif
            if(!aborted && device.Diagnostics().queue_healthy && journal.Healthy()){
                try{
                    if(!diagnostic_window.Owned()){
                        dg::Operation attachment("diagnostic.error_window_attachment");diagnostic_window.Acquire();
                    }
                    std::array<wchar_t,128> message{};
                    try{std::rethrow_exception(failure);}catch(const std::exception& error){const char* s=error.what();for(unsigned i=0;s[i] && i+1<message.size();++i)message[i]=s[i]>=32 && s[i]<127?wchar_t(s[i]):L'?';}
                    catch(...){std::wcscpy(message.data(),L"Original engine exception; inspect run.jsonl");}
                    const wchar_t* screen[]{L"MatrixGame diagnostic FAILED",phase,message.data(),L"Full details are in the run.jsonl log"};
                    dg::RenderStatus(device,screen);hold();
                }catch(...){dg::Record("unavailable","diagnostic.error_screen","could not display error; original failure retained");}
            }
            std::rethrow_exception(failure);
        }
    }
    dg::Record("ok","D.device_owner_released");
    diagnostic_window.Release();
    CheckLibnx(window.LastVisibilityRestoreResult(),"restore applet visibility");
}
}
int main(int argc,char** argv){
    std::unique_ptr<dg::Journal> journal;std::unique_ptr<dg::Binding> binding;int result=10;
    try{
        journal=std::make_unique<dg::Journal>("sdmc:/switch/matrixgame-diag/logs",matrixgame_diagnostic_build::Id,matrixgame_diagnostic_build::SourceSha256);
        binding=std::make_unique<dg::Binding>(*journal);ImageFingerprint(argc?argv[0]:nullptr);
        const char* resources=argc>1 && argv[1] && *argv[1]?argv[1]:"sdmc:/switch/matrixgame-diag/game";
        const char* rangers_resources=argc>2 && argv[2] && *argv[2]?argv[2]:"sdmc:/switch/space-rangers-hd-a-war-apart/game";
        dg::Record("metadata","resource_root",resources);
        dg::Record("metadata","session_mode",Interactive?"interactive":"quick-four-frames");
        dg::Record("metadata","exit_controls",Interactive?"Minus+Plus; B/Plus remain game input":"B or Plus");
        dg::Record("metadata","rangers_host",matrixgame_diagnostic_build::RangersHostEnabled?"enabled":"disabled");
        if(matrixgame_diagnostic_build::RangersHostEnabled)dg::Record("metadata","rangers_resource_root",rangers_resources);
        Run(resources,rangers_resources,*journal);journal->RequireHealthy();
        if(journal->Failures())throw std::runtime_error("diagnostic contains failed stages; inspect journal");
        dg::Record("ok","diagnostic_complete",Interactive?"manual original loop returned; original exit state recorded; not full battle acceptance":"four original map frames; SWITCH RUNTIME only if executed on hardware");result=0;
    }catch(const UserExit&){dg::Record("cancelled","user_exit","stack cleanup completed");result=20;}
    catch(const mg::applet_window::ExitRequested&){dg::Record("cancelled","applet_exit","stack cleanup completed");result=20;}
    catch(const Base::CException& error){
        try{const auto message=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(error.Info());dg::Record("exception","Base::CException",message);std::fprintf(stderr,"MatrixGame: %s\n",message.c_str());}catch(...){dg::Record("exception","Base::CException","failed to format original exception");}
    }catch(const std::exception& error){dg::Record("exception","std::exception",error.what());std::fprintf(stderr,"MatrixGame diagnostic: %s\n",error.what());}
    catch(...){dg::Record("exception","unknown_exception","stack cleanup completed");}
    if(journal){dg::Record("result","process_exit",{},result);if(!journal->Close())result=11;}
    return result;
}
