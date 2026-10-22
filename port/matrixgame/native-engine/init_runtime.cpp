// SPDX-License-Identifier: GPL-2.0-or-later
#include "init_runtime.hpp"
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
    if(active!=this || !cpu_owned_ || GpuReady())throw std::logic_error("device attachment requires this live CPU owner");
    if(window.Identity()!=InspectCpu3GState().window)throw std::logic_error("device window differs from CPU binding");
    const auto module=InspectCpu3GState().module;
    try {platform3g::InitAsDLL(module,window,device);}
    catch(...) {
        // Device rollback removes the hook/gamma. Retain this owner's CPU/cache
        // session, so failure does not turn CpuReady into a dangling identity.
        L3GInitAsDLLCpuPhase(module,window.Identity());
        throw;
    }
}
void NativeInitRuntime::Deinit() noexcept {
    if(cpu_owned_){L3GDeinitCpuPhase();cpu_owned_=false;}
    if(cache_owned_){CacheDeinit();cache_owned_=false;}
    scope_.reset();buffers_.reset();
    if(active==this)active=nullptr;
}
void NativeInitRuntime::DetachDevice() noexcept {
    if(active!=this || !GpuReady())return;
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
