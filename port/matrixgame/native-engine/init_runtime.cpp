// SPDX-License-Identifier: GPL-2.0-or-later
#include "init_runtime.hpp"
#include "MatrixSampleStateManager.hpp"
#include "device_defaults.hpp"
#include "game_defaults.hpp"
#include "MatrixCursor.hpp"
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include <stdexcept>
Base::CHeap* g_MatrixHeap;
CMatrixMapLogic* g_MatrixMap;
namespace srhd_awa::matrixgame {
using Base::CBlockPar;
namespace {NativeInitRuntime* active{};}
NativeInitRuntime::NativeInitRuntime()=default;
bool NativeCacheFactoryBound() noexcept {return active && active->CpuReady();}
NativeInitRuntime::~NativeInitRuntime(){Deinit();}
void NativeInitRuntime::Begin(void* module,void* window) {
    if(active || g_Cache || g_CacheHeap || g_MatrixHeap || g_MatrixData)throw std::logic_error("live engine/cache/game-heap/data owner already present");
    active=this;
    try {
        matrix_heap_=std::make_unique<Base::CHeap>();g_MatrixHeap=matrix_heap_.get();
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
    if(game_defaults_){ClearOriginalGameDefaults();game_defaults_=false;}
    if(cpu_owned_){DetachDevice();L3GDeinitCpuPhase();g_Sampler.m_AFDegree=0;cpu_owned_=false;}
    if(matrix_data_) {
        if(g_MatrixData==matrix_data_)g_MatrixData=nullptr;
        HDelete(CBlockPar,matrix_data_,g_MatrixHeap);matrix_data_=nullptr;
    }
    if(cache_owned_){CacheDeinit();cache_owned_=false;}
    scope_.reset();buffers_.reset();
    if(g_MatrixHeap==matrix_heap_.get())g_MatrixHeap=nullptr;
    matrix_heap_.reset();
    if(active==this)active=nullptr;
}
void NativeInitRuntime::InitializeCursor() {
    if(active!=this || !GpuReady() || !GameDefaultsReady())throw platform3g::Unsupported("cursor requires this negotiated window/device and original game defaults");
    if(cursor_)throw std::logic_error("cursor already owned by this session");
    cursor_=std::make_unique<CMatrixCursor>();
}
void NativeInitRuntime::InitializeGameDefaults() {
    if(active!=this || !CpuReady())throw platform3g::Unsupported("original config defaults require this live CPU/cache owner");
    if(game_defaults_)throw std::logic_error("game defaults already initialized by this owner");
    InitializeOriginalGameDefaults();game_defaults_=true;
}
void NativeInitRuntime::LoadGameData(const wchar_t* path) {
    if(active!=this || !CpuReady() || !GameDefaultsReady())throw std::logic_error("game data requires this CPU/config owner");
    if(g_MatrixData || matrix_data_)throw std::logic_error("existing game data owner");
    auto* data=HNew(g_MatrixHeap) CBlockPar{};
    try {data->LoadFromTextFile(path);}
    catch(...) {HDelete(CBlockPar,data,g_MatrixHeap);throw;}
    matrix_data_=data;g_MatrixData=data;
}
void NativeInitRuntime::ReadGameParameters() {
    if(active!=this || !GameDefaultsReady() || !matrix_data_ || g_MatrixData!=matrix_data_)
        throw std::logic_error("ReadParams requires this owner's real game data");
    g_Config.ReadParams();
}
void NativeInitRuntime::ApplyGameSettings(SRobotsSettings& settings) {
    if(active!=this || !GameDefaultsReady())throw std::logic_error("game settings require this CPU/config owner");
    g_Config.ApplySettings(&settings);
}
void NativeInitRuntime::ApplySamplerSettings(SRobotsSettings& settings) {
    if(active!=this || !GpuReady())throw platform3g::Unsupported("sampler settings require this live negotiated device");
    g_Sampler.ApplySettings(&settings); // Same original call as CGame::Init:323.
    sampler_configured_=true;defaults_applied_=false;
}
void NativeInitRuntime::ApplyVideoSettings(SRobotsSettings& settings) {
    if(active!=this || !CpuReady() || !GameDefaultsReady() || (GpuReady() && !sampler_configured_))
        throw platform3g::Unsupported("video settings require this CPU/config owner and negotiated sampler when a device is present");
    // Without a device, the original method itself rejects at ReadyDevice's
    // first creation-parameter query, before any mode/flag/presentation write.
    auto& state=platform3g::Current();
    const auto flags=state.flags;const auto presentation=state.presentation;
    // Current providers reject every window mutation and Reset before side
    // effects. Preserve the shared CPU values as well on that failure path.
    try {CGame::ApplyVideoParams(settings);}
    catch(...) {state.flags=flags;state.presentation=presentation;throw;}
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
    cursor_.reset(); // Original Clear must run before window/heap providers die.
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
