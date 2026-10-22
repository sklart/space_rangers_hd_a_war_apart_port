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
void NativeInitRuntime::Deinit() noexcept {
    if(cpu_owned_){L3GDeinitCpuPhase();cpu_owned_=false;}
    if(cache_owned_){CacheDeinit();cache_owned_=false;}
    scope_.reset();buffers_.reset();
    if(active==this)active=nullptr;
}
const char* NativeInitRuntime::FirstUnsupportedOperation() noexcept {
    return "L3GInitAsDLL: Win32 WndProc attachment, then IDirect3DDevice9::GetDeviceCaps; native full-device contract absent";
}
}
