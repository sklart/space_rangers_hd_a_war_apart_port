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
enum class EndReason {Completed,TimeLimit,UserExit};
struct LoopStop {EndReason reason;};
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
    std::uint64_t loop_started_{};
    bool polling_{true},in_loop_{};
public:
    Cancellation(){padInitializeDefault(&pad_);}
    void Poll(){
        if(!polling_)return;
        padUpdate(&pad_);const auto held=padGetButtons(&pad_);
        const auto result=control_.Poll(Now(),{bool(held&HidNpadButton_B),bool(held&HidNpadButton_Plus),bool(held&HidNpadButton_Minus)});
        if(result==dg::RunControl::PollResult::UserExit){
            if(in_loop_)throw LoopStop{EndReason::UserExit};
            throw UserExit();
        }
        if(result==dg::RunControl::PollResult::Deadline){
            if(in_loop_ && control_.HasControlledDeadline())throw LoopStop{EndReason::TimeLimit};
            throw Timeout();
        }
    }
    bool Presented(std::uint64_t before,std::uint64_t after,bool visible){return control_.Presented(before,after,visible);}
    std::uint64_t Frames()const noexcept{return control_.Frames();}
    void BeginOriginalLoop(){
        const auto now=Now();
        const auto result=control_.BeginOriginalLoop(now);
        if(result==dg::RunControl::PollResult::UserExit)throw UserExit();
        if(result==dg::RunControl::PollResult::Deadline)throw Timeout();
        loop_started_=now;
        in_loop_=true;
    }
    void EndLoop()noexcept{in_loop_=false;}
    void FinishPolling()noexcept{polling_=false;}
    std::uint64_t LoopElapsed()const{return Now()-loop_started_;}
    bool CountersDue()const noexcept{return control_.CountersDue();}
    bool ValidLoopReturn(int state)const noexcept{return control_.ValidLoopReturn(state);}
};
void Memory(){
    u64 total{},used{};const auto a=svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
    const auto b=svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
    dg::Record("result","process_memory.limit",{},a);if(R_SUCCEEDED(a))dg::Metric("process_memory_limit_bytes",total);
    dg::Record("result","process_memory.used",{},b);if(R_SUCCEEDED(b))dg::Metric("process_memory_used_bytes",used);
}
void JournalPhases(const dg::JournalIOStats& io){
    char detail[256];
    std::snprintf(detail,sizeof(detail),
        "write_calls=%llu write_us=%llu flush_calls=%llu flush_us=%llu sync_calls=%llu sync_us=%llu durable_rows=%llu",
        (unsigned long long)io.calls,(unsigned long long)io.write_us,
        (unsigned long long)io.flush_calls,(unsigned long long)io.flush_us,
        (unsigned long long)io.sync_calls,(unsigned long long)io.sync_us,(unsigned long long)io.durable_rows);
    dg::Record("profile","journal_io_phases",detail);
}
void Counters(p::DekoDevice& device,const dg::JournalIOStats* frame_io=nullptr){
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
    profile("state_validation",s.state_validation);
    profile("batch_gpu_wait",s.batch_gpu_wait);profile("present_gpu_wait",s.present_gpu_wait);
    profile("abort_gpu_wait",s.abort_gpu_wait);profile("acquire_gpu_wait",s.acquire_gpu_wait);
    const char* kinds[]{"draw","command_spill","command_fixed"};
    for(unsigned kind=0;kind<s.blocks.size();++kind)for(unsigned bucket=0;bucket<s.blocks[kind].size();++bucket){
        const auto& b=s.blocks[kind][bucket];
        if(!b.requests)continue;
        const auto limit=bucket<p::DekoDevice::BlockBucketLimits.size()?p::DekoDevice::BlockBucketLimits[bucket]:0;
        char detail[512];
        std::snprintf(detail,sizeof(detail),
            "kind=%s upper_bytes=%u requests=%llu requested=%llu reserved=%llu hits=%llu no_fit=%llu above_reuse_limit=%llu over_pool_budget=%llu create_calls=%llu create_us=%llu max_create_us=%llu repeat_bucket_creates=%llu",
            kinds[kind],limit,(unsigned long long)b.requests,(unsigned long long)b.requested_bytes,
            (unsigned long long)b.reserved_bytes,(unsigned long long)b.hits,(unsigned long long)b.miss_no_fit,
            (unsigned long long)b.miss_above_reuse_limit,(unsigned long long)b.miss_over_pool_budget,
            (unsigned long long)b.create.calls,(unsigned long long)b.create.us,(unsigned long long)b.create.maximum_us,
            (unsigned long long)(b.create.calls?b.create.calls-1:0));
        dg::Record("profile","gpu_block_bucket",detail);
    }
    const auto io=frame_io?*frame_io:dg::CurrentIOStats();
    dg::Metric("profile.journal.calls",io.calls);dg::Metric("profile.journal.bytes",io.bytes);dg::Metric("profile.journal.us",io.us);
    dg::Metric("gpu_buffer_bytes_cumulative",s.buffer_bytes_total);dg::Metric("recorded_draw_calls",s.recorded_draws);
    dg::Metric("queue_submission_calls",s.queue_submissions);dg::Metric("completed_presents",s.presents);
    dg::Metric("native_texture_session_uploads",s.session_uploads);dg::Metric("texture_arena_capacity",s.texture_capacity);
    dg::Metric("batch_drains",s.batch_drains);dg::Metric("max_live_draw_packets",s.max_live_draw_packets);dg::Metric("max_live_draw_bytes",s.max_live_draw_bytes);
    dg::Metric("batch_explicit_flush_calls",s.batch_explicit_flush_calls);
    dg::Metric("managed_texture_evictions",device.TextureSession()->managed_evictions);
    dg::Metric("texture_arena_free",s.texture_free);dg::Metric("queue_healthy",s.queue_healthy);Memory();
    dg::Metric("SDK_warnings_total",s.sdk_warnings);dg::Metric("SDK_warning_records_suppressed",s.sdk_warnings>32?s.sdk_warnings-32:0);
    JournalPhases(io);
    for(unsigned i=1;i<s.batch_wait_by_cause.size();++i){
        const auto& stats=s.batch_wait_by_cause[i];
        if(!stats.wait.calls)continue;
        char detail[512];
        const auto& h=stats.histogram;
        std::snprintf(detail,sizeof(detail),
            "cause=%s calls=%llu us=%llu max_us=%llu le250=%llu le1000=%llu le4000=%llu le16000=%llu gt16000=%llu max_draws=%llu max_upload_bytes=%llu max_command_backing_bytes=%llu max_command_spills=%llu",
            p::BatchWaitNames[i],(unsigned long long)stats.wait.calls,(unsigned long long)stats.wait.us,
            (unsigned long long)stats.wait.maximum_us,(unsigned long long)h[0],(unsigned long long)h[1],
            (unsigned long long)h[2],(unsigned long long)h[3],(unsigned long long)h[4],
            (unsigned long long)stats.draws_max,(unsigned long long)stats.upload_bytes_max,
            (unsigned long long)stats.command_backing_bytes_max,(unsigned long long)stats.command_spills_max);
        dg::Record("profile","batch_gpu_wait_cause",detail);
    }
    for(const auto& [name,stats]:std::array{
        std::pair{"reset_gpu_wait",s.reset_gpu_wait},std::pair{"gamma_readback_gpu_wait",s.gamma_readback_gpu_wait},
        std::pair{"screenshot_gpu_wait",s.screenshot_gpu_wait}})
        if(stats.calls)profile(name,stats);
    char detail[320];
    std::snprintf(detail,sizeof(detail),
        "draw_limit=%llu upload_limit=%llu command_backing_limit=%u queue_command_bytes=%u queue_flush_threshold=%u max_pool_bytes_at_retirement=%llu max_pool_blocks_at_retirement=%llu",
        (unsigned long long)p::NativeDrawBatch::DrawLimit,(unsigned long long)p::NativeDrawBatch::ByteLimit,
        s.command_backing_limit,s.queue_command_bytes,s.queue_flush_threshold,
        (unsigned long long)s.max_pool_bytes,(unsigned long long)s.max_pool_blocks);
    dg::Record("profile","batch_memory_limits",detail);
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
    mg::native_loop::SwitchBackend actual_;p::DekoDevice& device_;Cancellation& cancel_;dg::Journal& journal_;
    bool batch_requested_,draw_drain_sdk_flush_,batch_active_{},capture_requested_{};std::uint64_t transition_presents_{};
public:
    BoundedOriginalLoop(p::SwitchWindow& w,p::DekoDevice& d,Cancellation& c,dg::Journal& j,bool batch,bool draw_drain_sdk_flush)
        :actual_(w,d),device_(d),cancel_(c),journal_(j),batch_requested_(batch),draw_drain_sdk_flush_(draw_drain_sdk_flush){cancel_.BeginOriginalLoop();}
    ~BoundedOriginalLoop(){device_.SetDrawDrainSdkFlush(false);EndJournalBatch();}
    void EndJournalBatch()noexcept{
        if(batch_active_){journal_.EndBatching();batch_active_=false;}
    }
    void Pump()override{cancel_.Poll();actual_.Pump();cancel_.Poll();}
    void Idle()override{actual_.Idle();}
    void Draw(CForm& form)override{
        cancel_.Poll();
        const bool visible=(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY) && !(g_MatrixMap->m_Flags&MMFLAG_TRANSITION);
        device_.SetDrawDrainSdkFlush(visible && draw_drain_sdk_flush_);
        if(batch_requested_ && visible && !batch_active_){journal_.BeginBatching();batch_active_=true;}
        if(!visible)EndJournalBatch();
        dg::Operation operation("C.original_form.Draw");const auto before=device_.Diagnostics().presents;
        if(visible && !capture_requested_){device_.RequestPrimaryReadback([output=&journal_](auto bytes,unsigned w,unsigned h){dg::SaveFrame(*output,"original-first",w,h,bytes);});capture_requested_=true;}
        try{actual_.Draw(form);}
        catch(const LoopStop&){operation.Cancel();throw;}
        const auto after=device_.Diagnostics().presents;const auto done=cancel_.Presented(before,after,visible);
        if(!visible && after>before){transition_presents_+=after-before;dg::Metric("original_loading_presents",transition_presents_);if(transition_presents_<=4 || transition_presents_%30==0)Counters(device_);}
        const bool snapshot=visible && after>before && cancel_.CountersDue();
        if(batch_active_ && !journal_.Checkpoint(snapshot))journal_.RequireHealthy();
        if(snapshot){
            // Both modes count physical I/O at the same boundary. Complete any
            // pending batch before the marker so its I/O belongs to this window.
            const auto io=journal_.Statistics();
            dg::Metric("original_loop_frames_presented",cancel_.Frames());Counters(device_,&io);
        }
        if(done)g_Flags|=GFLAG_EXITLOOP;
    }
    std::uint64_t Frames()const noexcept{return cancel_.Frames();}
    bool ValidLoopReturn(int state)const noexcept{return cancel_.ValidLoopReturn(state);}
};
EndReason Run(const char* root_arg,const char* rangers_root_arg,dg::Journal& journal,bool batch_journal,bool draw_drain_sdk_flush){
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
    EndReason reason=EndReason::Completed;
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
            BoundedOriginalLoop backend(window,device,cancel,journal,batch_journal,draw_drain_sdk_flush);mg::native_loop::Scope loop(backend);
            g_ExitState=0;
            g_Flags&=~GFLAG_EXITLOOP;
            try{RunOriginalNativeLoop();}
            catch(const LoopStop& stop){reason=stop.reason;}
            backend.EndJournalBatch();
            journal.RequireHealthy();
            cancel.EndLoop();
            const auto elapsed=cancel.LoopElapsed();
            dg::Metric("original_loop_frames_presented_final",backend.Frames());
            dg::Metric("original_loop_elapsed_us_final",elapsed);
            dg::Metric("original_loop_fps_milli_final",elapsed?backend.Frames()*1'000'000'000ull/elapsed:0);
            if(reason==EndReason::Completed){
                if(!backend.ValidLoopReturn(g_ExitState))throw std::runtime_error("original loop ended without valid diagnostic frames/exit state");
                dg::Metric("original_exit_state",g_ExitState);
            }
        }
        if(reason==EndReason::Completed){
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
        if(reason==EndReason::Completed && !Interactive)hold();
        cancel.FinishPolling();
        phase=L"D: original cleanup";
        if(reason!=EndReason::Completed){
            const auto aborted=device.AbortNativeFrame();
            dg::Record(aborted?"failed":"result","diagnostic.stop_abort",{},aborted);
            if(aborted)throw std::runtime_error("controlled stop GPU completion failed");
        }
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
            rangers_host.reset();dg::Record("ok","D.RangersHostSession.error");
#endif
            if(!aborted && window.Drawable() && device.NativeCooperativeLevel()==p::DeviceSuccess && journal.Healthy()){
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
    return reason;
}
}
int main(int argc,char** argv){
    std::unique_ptr<dg::Journal> journal;std::unique_ptr<dg::Binding> binding;int result=10;
    try{
        journal=std::make_unique<dg::Journal>("sdmc:/switch/matrixgame-diag/logs",matrixgame_diagnostic_build::Id,matrixgame_diagnostic_build::SourceSha256);
        binding=std::make_unique<dg::Binding>(*journal);ImageFingerprint(argc?argv[0]:nullptr);
        const char* roots[]{"sdmc:/switch/matrixgame-diag/game","sdmc:/switch/space-rangers-hd-a-war-apart/game"};
        unsigned positional=0;bool mode_set=false,batch_journal=false,drain_mode_set=false,draw_drain_sdk_flush=false;
        for(int i=1;i<argc;++i){
            const std::string_view arg=argv[i];
            if(arg.starts_with("--drain-flush=")){
                if(drain_mode_set || (arg!="--drain-flush=explicit" && arg!="--drain-flush=sdk"))
                    throw std::invalid_argument("expected one --drain-flush=explicit or --drain-flush=sdk");
                drain_mode_set=true;draw_drain_sdk_flush=arg=="--drain-flush=sdk";
            }else if(arg.starts_with("--")){
                if(mode_set || (arg!="--journal=sync" && arg!="--journal=batch"))
                    throw std::invalid_argument("expected one --journal=sync or --journal=batch");
                mode_set=true;batch_journal=arg=="--journal=batch";
            }else{
                if(positional==std::size(roots))throw std::invalid_argument("expected at most two resource paths");
                if(!arg.empty())roots[positional]=argv[i];
                ++positional;
            }
        }
        if(drain_mode_set && !mode_set)batch_journal=true;
        if((batch_journal || drain_mode_set) && !Interactive)throw std::invalid_argument("batch journal and drain A/B require Interactive");
        const auto* resources=roots[0];const auto* rangers_resources=roots[1];
        dg::Record("metadata","journal_mode",batch_journal?"batch":"sync");
        dg::Record("metadata","draw_drain_flush",draw_drain_sdk_flush?"sdk":"explicit");
        dg::Record("metadata","resource_root",resources);
        dg::Record("metadata","session_mode",Interactive?"interactive":"quick-four-frames");
        dg::Record("metadata","exit_controls",Interactive?"Minus+Plus; B/Plus remain game input":"B or Plus");
        dg::Record("metadata","rangers_host",matrixgame_diagnostic_build::RangersHostEnabled?"enabled":"disabled");
        if(matrixgame_diagnostic_build::RangersHostEnabled)dg::Record("metadata","rangers_resource_root",rangers_resources);
        const auto reason=Run(resources,rangers_resources,*journal,batch_journal,draw_drain_sdk_flush);journal->RequireHealthy();
        if(journal->Failures())throw std::runtime_error("diagnostic contains failed stages; inspect journal");
        if(reason==EndReason::Completed){
            dg::Record("ok","diagnostic_complete",Interactive?"manual original loop returned; original exit state recorded; not full battle acceptance":"four original map frames; SWITCH RUNTIME only if executed on hardware");
            result=0;
        }else{
            dg::Record("metadata","session_end_reason",reason==EndReason::TimeLimit?"time_limit":"user_exit");
            dg::Record("controlled_stop","diagnostic_complete","CONTROLLED_STOP");
            result=reason==EndReason::TimeLimit?0:20;
        }
    }catch(const UserExit&){dg::Record("cancelled","user_exit","stack cleanup completed");result=20;}
    catch(const mg::applet_window::ExitRequested&){dg::Record("cancelled","applet_exit","stack cleanup completed");result=20;}
    catch(const Base::CException& error){
        try{const auto message=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(error.Info());dg::Record("exception","Base::CException",message);std::fprintf(stderr,"MatrixGame: %s\n",message.c_str());}catch(...){dg::Record("exception","Base::CException","failed to format original exception");}
    }catch(const std::exception& error){dg::Record("exception","std::exception",error.what());std::fprintf(stderr,"MatrixGame diagnostic: %s\n",error.what());}
    catch(...){dg::Record("exception","unknown_exception","stack cleanup completed");}
    if(journal){
        JournalPhases(journal->Statistics());
        dg::Record("result","process_exit",{},result);
        if(!journal->Close()){
            std::fprintf(stderr,"MatrixGame diagnostic journal failed: %d\n",journal->Error());
            result=11;
        }
    }
    return result;
}
