// SPDX-License-Identifier: GPL-2.0-or-later
#include "init_runtime.hpp"
#include "MatrixSampleStateManager.hpp"
#include "device_defaults.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {NativeInitRuntime* active{};}
bool NativeCacheFactoryBound() noexcept {return active && active->CpuReady();}
NativeInitRuntime::~NativeInitRuntime(){Deinit();}
void NativeInitRuntime::Begin(void* module,void* window) {
    if(active || g_Cache || g_CacheHeap)throw std::logic_error("live engine/cache owner already present");
    active=this;
    try {
        buffers_=std::make_unique<buffers::Context>();
        scope_=std::make_unique<buffers::Scope>(*buffers_);
        CacheInit();cache_owned_=true;
        L3GInitAsDLLCpuPhase(module,window);cpu_owned_=true;
    } catch(...) {Deinit();throw;}
}
void NativeInitRuntime::AttachDevice(platform3g::Window& window,platform3g::Device& device) {
    if(active!=this || !cpu_owned_ || platform3g::Current().ready || device_snapshot_)throw std::logic_error("device attachment requires this live CPU owner");
    if(window.Identity()!=InspectCpu3GState().window)throw std::logic_error("device window differs from CPU binding");
    const auto module=InspectCpu3GState().module;
    last_restore_.reset();sampler_configured_=false;defaults_applied_=false;
    try {
        platform3g::InitAsDLL(module,window,device);
        device_snapshot_=device.CaptureState();
        if(!device_snapshot_)throw std::logic_error("device returned no state lease");
        g_Sampler.m_AFDegree=0;
    }
    catch(...) {
        // Device rollback removes the hook/gamma. Retain this owner's CPU/cache
        // session, so failure does not turn CpuReady into a dangling identity.
        device_snapshot_.reset();platform3g::Deinit();
        L3GInitAsDLLCpuPhase(module,window.Identity());
        throw;
    }
}
void NativeInitRuntime::Deinit() noexcept {
    if(cpu_owned_){DetachDevice();L3GDeinitCpuPhase();g_Sampler.m_AFDegree=0;cpu_owned_=false;}
    if(cache_owned_){CacheDeinit();cache_owned_=false;}
    scope_.reset();buffers_.reset();
    if(active==this)active=nullptr;
}
void NativeInitRuntime::ApplySamplerSettings(SRobotsSettings& settings) {
    if(active!=this || !GpuReady())throw platform3g::Unsupported("sampler settings require this live negotiated device");
    g_Sampler.ApplySettings(&settings); // Same original call as CGame::Init:323.
    sampler_configured_=true;defaults_applied_=false;
}
void NativeInitRuntime::ApplyDefaultDeviceState() {
    if(active!=this || !GpuReady() || !sampler_configured_)
        throw platform3g::Unsupported("S3D_Default requires this owner's device and original sampler settings");
    defaults_applied_=false;
    try {S3D_Default();defaults_applied_=true;}
    catch(...) {DetachDevice();throw;}
}
void NativeInitRuntime::DetachDevice() noexcept {
    if(active!=this || !cpu_owned_)return;
    const bool was_bound=platform3g::Current().ready;
    if(device_snapshot_){last_restore_=device_snapshot_->Restore();device_snapshot_.reset();}
    sampler_configured_=false;defaults_applied_=false;g_Sampler.m_AFDegree=0;
    if(!was_bound)return;
    const auto module=platform3g::Current().module;
    const auto window=platform3g::Current().window_identity;
    platform3g::Deinit();
    // Preserve the existing CPU/cache identity without allocating/reinitializing it.
    auto& state=platform3g::Current();state.module=module;state.window_identity=window;state.external=true;
}
const char* NativeInitRuntime::FirstUnsupportedOperation() noexcept {
    return "L3GInitAsDLL requires a real Window/Device provider; deko legacy caps and CGame::Init dependency closure remain unsupported";
}
}
