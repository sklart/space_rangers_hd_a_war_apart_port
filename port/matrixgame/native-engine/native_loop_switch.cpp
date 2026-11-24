// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_loop_switch.hpp"
#include "MatrixMap.hpp"
#include "Form.hpp"
#include "native_frame_profiler.hpp"
#include <switch.h>
#include <cstdio>
namespace srhd_awa::matrixgame::native_loop {
void SwitchBackend::Pump(){window_.Update();}
void SwitchBackend::Idle(){svcSleepThread(YieldType_WithoutCoreMigration);}
void SwitchBackend::Draw(CForm& form){
    // The original form skips Draw while its recovery has not recreated video
    // resources. Do not acquire a display image in that state.
    if(!(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY)){form.Draw();return;}
    struct DrawFlushScope {
        platform3g::DekoDevice& device;
        explicit DrawFlushScope(platform3g::DekoDevice& d):device(d){device.SetDrawDrainSdkFlush(true);}
        ~DrawFlushScope(){device.SetDrawDrainSdkFlush(false);}
    } flush_scope{device_};
    struct Rollback {
        platform3g::DekoDevice& device;
        ~Rollback(){const auto result=device.AbortNativeFrame();if(result)std::fprintf(stderr,"native loop frame rollback failed: %08x\n",unsigned(result));}
    } rollback{device_};
    device_.AcquireNativeFrame();
    form.Draw(); // Original Clear/BeginScene/map.Draw/EndScene/Present.
    frame_profiler::ReportIfDue();
    const auto now=std::chrono::steady_clock::now();
    if(report_start_==std::chrono::steady_clock::time_point{}){
        report_start_=now;report_base_=device_.Diagnostics();report_frames_=0;
    }else{
        ++report_frames_;
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now-report_start_).count();
        if(ms>=5000){
            const auto d=device_.Diagnostics();const auto& b=report_base_;
            std::fprintf(stderr,"[MATRIX_GPU] window_ms=%lld frames=%llu draws=%llu drains=%llu arena_fills=%llu arena_fallbacks=%llu submissions=%llu explicit_flushes=%llu vertex_layout_reuses=%llu viewport_reuses=%llu scissor_reuses=%llu multisample_reuses=%llu sampler_descriptor_reuses=%llu sampler_set_reuses=%llu image_descriptor_reuses=%llu material_binding_reuses=%llu raster_reuses=%llu color_reuses=%llu line_reuses=%llu depth_reuses=%llu draw_cpu_us=%llu decode_us=%llu encode_us=%llu batch_wait_us=%llu present_wait_us=%llu acquire_wait_us=%llu abort_wait_us=%llu\n",
                (long long)ms,(unsigned long long)report_frames_,
                (unsigned long long)(d.recorded_draws-b.recorded_draws),(unsigned long long)(d.batch_drains-b.batch_drains),(unsigned long long)(d.upload_arena_fills-b.upload_arena_fills),(unsigned long long)(d.upload_arena_fallbacks-b.upload_arena_fallbacks),
                (unsigned long long)(d.queue_submissions-b.queue_submissions),(unsigned long long)(d.batch_explicit_flush_calls-b.batch_explicit_flush_calls),
                (unsigned long long)(d.vertex_layout_reuses-b.vertex_layout_reuses),(unsigned long long)(d.viewport_reuses-b.viewport_reuses),(unsigned long long)(d.scissor_reuses-b.scissor_reuses),(unsigned long long)(d.multisample_reuses-b.multisample_reuses),(unsigned long long)(d.sampler_descriptor_reuses-b.sampler_descriptor_reuses),(unsigned long long)(d.sampler_set_reuses-b.sampler_set_reuses),(unsigned long long)(d.image_descriptor_reuses-b.image_descriptor_reuses),(unsigned long long)(d.material_binding_reuses-b.material_binding_reuses),(unsigned long long)(d.raster_reuses-b.raster_reuses),(unsigned long long)(d.color_reuses-b.color_reuses),(unsigned long long)(d.line_reuses-b.line_reuses),(unsigned long long)(d.depth_reuses-b.depth_reuses),(unsigned long long)(d.draw_cpu.us-b.draw_cpu.us),
                (unsigned long long)(d.vertex_decode.us-b.vertex_decode.us),(unsigned long long)(d.command_encode.us-b.command_encode.us),
                (unsigned long long)(d.batch_gpu_wait.us-b.batch_gpu_wait.us),(unsigned long long)(d.present_gpu_wait.us-b.present_gpu_wait.us),
                (unsigned long long)(d.acquire_gpu_wait.us-b.acquire_gpu_wait.us),(unsigned long long)(d.abort_gpu_wait.us-b.abort_gpu_wait.us));
            for(std::size_t i=1;i<d.batch_wait_by_cause.size();++i){
                const auto& current=d.batch_wait_by_cause[i].wait;const auto& previous=b.batch_wait_by_cause[i].wait;
                if(current.calls!=previous.calls)std::fprintf(stderr,"[MATRIX_GPU_CAUSE] cause=%s calls=%llu wait_us=%llu\n",
                    platform3g::BatchWaitNames[i],(unsigned long long)(current.calls-previous.calls),
                    (unsigned long long)(current.us-previous.us));
            }
            report_start_=now;report_base_=d;report_frames_=0;
        }
    }
}
}
