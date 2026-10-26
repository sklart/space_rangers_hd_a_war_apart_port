// SPDX-License-Identifier: GPL-2.0-or-later
#include "init_runtime.hpp"
#include "native_frame_scope.hpp"
#include "resource_paths.hpp"
#include <cstdio>
#include "cache_owner_state.hpp"
#include "MatrixSampleStateManager.hpp"
#include "device_defaults.hpp"
#include "game_defaults.hpp"
#include "game_statics.hpp"
#include "MatrixCursor.hpp"
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include "MatrixLoadProgress.hpp"
#include "Texture.hpp"
#include "legacy_texture_backend.hpp"
#include "MatrixRenderPipeline.hpp"
#include "MatrixCamera.hpp"
#include "3g.hpp"
#include "Text/Font.hpp"
#include <cmath>
#include <stdexcept>
Base::CHeap* g_MatrixHeap;
CMatrixMapLogic* g_MatrixMap;
namespace srhd_awa::matrixgame {
using Base::CBlockPar;
namespace {NativeInitRuntime*& active=cache_owner_detail::Owner();}
NativeInitRuntime::NativeInitRuntime()=default;
NativeInitRuntime::~NativeInitRuntime(){Deinit();}
void NativeInitRuntime::Begin(void* module,void* window) {
    BeginSession(module,window,false);
}
void NativeInitRuntime::BeginSession(void* module,void* window,bool original_game) {
    if(active || g_Cache || g_CacheHeap || g_MatrixHeap || g_MatrixData || g_Render || g_MatrixMap || g_IFaceList || g_LoadProgress || g_FormFirst || g_FormLast || g_FormCur || legacy_texture::RegistrySize())
        throw std::logic_error("live engine/cache/game-heap/data/form owner already present");
    active=this;
    try {
        buffers_=std::make_unique<buffers::Context>();
        scope_=std::make_unique<buffers::Scope>(*buffers_);
        if(!original_game){
            InitializeOriginalGameStatics();statics_owned_=true;
            matrix_heap_=std::make_unique<Base::CHeap>();g_MatrixHeap=matrix_heap_.get();
            CacheInit();cache_owned_=true;
        }
        L3GInitAsDLLCpuPhase(module,window);cpu_owned_=true;
        if(original_game){cache_owner_detail::OriginalGameOwner()=this;cache_owner_detail::OriginalGameInitialized()=false;}
    } catch(...) {Deinit();throw;}
}
void NativeInitRuntime::AttachDevice(platform3g::Window& window,platform3g::Device& device) {
    AttachDeviceImpl(window,device,false);
}
void NativeInitRuntime::AttachDeviceImpl(platform3g::Window& window,platform3g::Device& device,bool native) {
    if(active!=this || !cpu_owned_ || platform3g::Current().ready || device_snapshot_)throw std::logic_error("device attachment requires this live CPU owner");
    if(window.Identity()!=InspectCpu3GState().window)throw std::logic_error("device window differs from CPU binding");
    const auto module=InspectCpu3GState().module;
    last_restore_.reset();sampler_configured_=false;defaults_applied_=false;
    try {
        if(native)platform3g::InitAsNative(window,device);
        else platform3g::InitAsDLL(module,window,device);
        device_snapshot_=device.CaptureState();
        if(!device_snapshot_)throw std::logic_error("device returned no state lease");
        legacy_window_=&window;legacy_device_=&device;
        g_Sampler.m_AFDegree=0;
    }
    catch(...) {
        // Device rollback removes the hook/gamma. Retain this owner's CPU/cache
        // session, so failure does not turn CpuReady into a dangling identity.
        device_snapshot_.reset();platform3g::Deinit();legacy_window_=nullptr;legacy_device_=nullptr;
        L3GInitAsDLLCpuPhase(module,window.Identity());
        throw;
    }
}
void NativeInitRuntime::BindLegacyProviders(platform3g::Window& window,platform3g::Device& device) {
    if(active!=this || !CpuReady() || platform3g::Current().ready || device_snapshot_)
        throw std::logic_error("legacy provider binding requires this cold CPU owner");
    if(window.Identity()!=InspectCpu3GState().window)throw std::invalid_argument("legacy window differs from CPU binding");
    legacy_window_=&window;legacy_device_=&device;
}
void NativeInitRuntime::EnterLegacyDll(void* module,void* window,std::uintptr_t api,std::uintptr_t device) {
    if(!active || !active->CpuReady())throw platform3g::Unsupported("legacy DLL entry requires an active native CPU owner");
    auto& owner=*active;
    if(!owner.legacy_window_ || !owner.legacy_device_)
        throw platform3g::Unsupported("legacy DLL entry requires registered borrowed Window/Device providers");
    const auto cpu=InspectCpu3GState();
    if(module!=cpu.module || window!=cpu.window || window!=owner.legacy_window_->Identity())
        throw std::invalid_argument("legacy caller module/window differs from registered CPU owner");
    const auto handles=owner.legacy_device_->ReadLegacyHandles();
    if(!handles.api || !handles.device || handles.api!=api || handles.device!=device)
        throw std::invalid_argument("legacy caller API/device differs from actual borrowed SDK identities");
    // Validate opaque caller identities before touching any live hook, cursor,
    // gamma or state lease. Never dereference arbitrary raw ABI arguments.
    auto& borrowed_window=*owner.legacy_window_;auto& borrowed_device=*owner.legacy_device_;
    owner.DetachDevice();
    owner.AttachDevice(borrowed_window,borrowed_device); // Original typed DLL algorithm + real state capture.
}
void NativeInitRuntime::LeaveLegacyDll() {
    if(active && active->CpuReady()){active->DetachDevice();return;}
    if(platform3g::Current().ready)throw platform3g::Unsupported("legacy deinit cannot detach a foreign platform owner");
}
void NativeInitRuntime::EnterNativePlatform(void* module,void* window) {
    if(!active || !active->CpuReady())throw platform3g::Unsupported("native entry requires active CPU owner");
    auto& owner=*active;const auto cpu=InspectCpu3GState();
    if(!owner.legacy_window_ || !owner.legacy_device_ || module!=cpu.module || window!=cpu.window ||
       window!=owner.legacy_window_->Identity())throw std::invalid_argument("native entry owner/window mismatch");
    owner.AttachDeviceImpl(*owner.legacy_window_,*owner.legacy_device_,true);
}
void NativeInitRuntime::Deinit() noexcept {
    // Original Init itself may draw/present its transition before returning.
    // Quiesce/cancel its acquired work before releasing map/font/resources.
    if(auto* native=dynamic_cast<platform3g::NativeFrameRollback*>(legacy_device_)){
        const auto result=native->AbortNativeFrame();if(result)std::fprintf(stderr,"native Init/frame cleanup failed: %08x\n",unsigned(result));
    }
    FinishOriginalGame();
    FinishMapCamera();
    FinishLoadProgress();
    FinishRenderPipeline();
    FinishOriginalManagedTexture();
    if(game_defaults_){ClearOriginalGameDefaults();game_defaults_=false;}
    if(cpu_owned_){DetachDevice();L3GDeinitCpuPhase();g_Sampler.m_AFDegree=0;cpu_owned_=false;}
    if(matrix_data_) {
        if(g_MatrixData==matrix_data_)g_MatrixData=nullptr;
        HDelete(CBlockPar,matrix_data_,g_MatrixHeap);matrix_data_=nullptr;
    }
    if(cache_owned_){CacheDeinit();cache_owned_=false;}
    legacy_window_=nullptr;legacy_device_=nullptr;
    scope_.reset();buffers_.reset();
    if(g_MatrixHeap==matrix_heap_.get())g_MatrixHeap=nullptr;
    matrix_heap_.reset();
    if(statics_owned_){Base::CMain::BaseDeInit();statics_owned_=false;}
    output_scope_.reset(); // Original map/font/cache teardown has finished.
    resource_scope_.reset(); // CFile package/disk owners have finished; restore legacy relative reads.
    if(active==this)active=nullptr;
}
void NativeInitRuntime::InitializeCursor() {
    if(active!=this || !GpuReady() || !GameDefaultsReady())throw platform3g::Unsupported("cursor requires this negotiated window/device and original game defaults");
    if(cursor_)throw std::logic_error("cursor already owned by this session");
    cursor_=std::make_unique<CMatrixCursor>();
}
void NativeInitRuntime::InitializeLoadProgress() {
    if(active!=this || !CpuReady() || !OriginalStaticsReady())throw std::logic_error("progress requires this initialized CPU owner");
    if(g_LoadProgress || load_progress_)throw std::logic_error("live load-progress publisher already present");
    auto progress=std::make_unique<CLoadProgress>();
    g_LoadProgress=progress.get();load_progress_=std::move(progress);
}
void NativeInitRuntime::FinishLoadProgress()noexcept {load_progress_.reset();}
void NativeInitRuntime::InitializeRenderPipeline() {
    if(active!=this || !CpuReady() || !OriginalStaticsReady())throw std::logic_error("renderer requires this initialized CPU owner");
    if(g_Render || render_pipeline_)throw std::logic_error("live renderer publisher already present");
    auto* renderer=HNew(g_MatrixHeap) CRenderPipeline;
    g_Render=renderer;render_pipeline_=renderer;
}
void NativeInitRuntime::FinishRenderPipeline()noexcept {
    if(!render_pipeline_)return;
    if(g_Render==render_pipeline_)g_Render=nullptr;
    HDelete(CRenderPipeline,render_pipeline_,g_MatrixHeap);render_pipeline_=nullptr;
}
void NativeInitRuntime::InitializeMapCamera(const SRobotsSettings& settings) {
    if(active!=this || !GameDefaultsReady())throw std::logic_error("map camera requires this CPU/config owner");
    if(map_camera_)throw std::logic_error("map camera already owned");
    // These are configured dimensions, not measured physical display facts.
    // Validate before the original constructor divides by either dimension.
    const double far_plane=4000.0*(1.0+0.01*settings.m_MaxDistance);
    if(settings.m_ResolutionX<=0 || settings.m_ResolutionY<=0 ||
       !std::isfinite(far_plane) || far_plane<=1.0 || far_plane>3.402823466e38)
        throw std::invalid_argument("invalid configured map camera projection");
    const int old_x=g_ScreenX,old_y=g_ScreenY;const float old_far=MAX_VIEW_DISTANCE;
    g_ScreenX=settings.m_ResolutionX;g_ScreenY=settings.m_ResolutionY;
    SetMaxCameraDistance(settings.m_MaxDistance); // Original CGame::Init prerequisite.
    try {map_camera_=HNew(g_MatrixHeap) CMatrixCamera;}
    catch(...) {g_ScreenX=old_x;g_ScreenY=old_y;MAX_VIEW_DISTANCE=old_far;throw;}
}
void NativeInitRuntime::FinishMapCamera()noexcept {
    if(map_camera_){HDelete(CMatrixCamera,map_camera_,g_MatrixHeap);map_camera_=nullptr;}
}
void NativeInitRuntime::LoadOriginalManagedTexture(const wchar_t* path,bool convert_to_16bit) {
    if(active!=this || !CpuReady() || !OriginalStaticsReady())throw std::logic_error("original managed texture requires this live CPU/cache owner");
    if(original_texture_)throw std::logic_error("original managed texture already owned");
    if(!path || !*path)throw std::invalid_argument("empty original texture path");
    auto* texture=legacy_texture::CreateManaged();
    try {texture->m_Name=path;texture->Init(convert_to_16bit);}
    catch(...) {CCache::Destroy(texture);throw;}
    original_texture_=texture;
}
void NativeInitRuntime::FinishOriginalManagedTexture()noexcept {
    if(original_texture_){CCache::Destroy(original_texture_);original_texture_=nullptr;}
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
    // Preserve scalar configuration on failure; the owner device lease and
    // Deinit restore the actual presentation when this session unwinds.
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
    if(device_snapshot_)Text::ClearFontCache(); // Release this negotiated device's text resources.
    FinishRenderPipeline();
    FinishOriginalManagedTexture(); // Actual SDK resource dies before device/window/cache teardown.
    legacy_window_=nullptr;legacy_device_=nullptr;
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
// Genuine legacy ABI used by the original CGame::Init/SafeFree call sites.
// Config/names are unused by the original DLL implementation too. Raw SDK
// arguments select no implementation; registered providers must prove identity.
void L3GInitAsDLL(void* module,Base::CBlockPar&,const wchar_t*,const wchar_t*,void* window,std::uintptr_t api,std::uintptr_t device) {
    srhd_awa::matrixgame::NativeInitRuntime::EnterLegacyDll(module,window,api,device);
}
void L3GDeinit() {
    srhd_awa::matrixgame::NativeInitRuntime::LeaveLegacyDll();
}
