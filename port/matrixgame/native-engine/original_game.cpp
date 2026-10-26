// SPDX-License-Identifier: GPL-2.0-or-later
#include "init_runtime.hpp"
#include "cache_owner_state.hpp"
#include "MatrixGame.h"
#include "Texture.hpp"
#include "3g.hpp"
#include "MatrixSkinManager.hpp"
#include "MatrixMap.hpp"
#include "skin_callbacks.hpp"
#include "map_device_backend.hpp"
#include "resource_paths.hpp"
#ifdef _WIN32
#include "platform_win32.hpp"
#endif
#include <iostream>
namespace srhd_awa::matrixgame {
namespace {
thread_local bool native_entry{};
thread_local NativeDrawSink* native_sink{};
std::unique_ptr<BorrowedSkinScope> original_skin_scope;
struct OriginalWindowsSink final:NativeDrawSink {
    void SetVertexLayout(std::uint32_t layout)override {
        platform3g::CheckDeviceResult(platform3g::ReadyDevice().SetFixedVertexLayout(layout),"original VO FVF");
    }
    void Emit(const MeshRange& range)override {
        if(range.index_count%3)throw std::invalid_argument("original VO triangle index count");
        platform3g::CheckDeviceResult(map_device::DrawIndexedPrimitive(4,range.base_vertex,range.minimum_vertex,
            range.vertex_count,range.first_index,range.index_count/3),"original VO DrawIndexedPrimitive");
    }
} original_windows_sink;
std::unique_ptr<NativeDrawScope> original_draw_scope;
}
namespace original_game {
void EnterPlatform(void* module,Base::CBlockPar& config,const wchar_t* name,const wchar_t* title,
                   void* window,std::uintptr_t api,std::uintptr_t device){
    if(!native_entry){L3GInitAsDLL(module,config,name,title,window,api,device);return;}
    if(api || device)throw std::invalid_argument("native entry rejects legacy D3D identities");
    NativeInitRuntime::EnterNativePlatform(module,window);
}
void ObserveStage(const char* stage){
    std::clog<<"original Init stage: "<<stage<<" borrowed_skin_objects="<<BorrowedSkinLeaseCount();
    if(g_MatrixMap)std::clog<<" map_size="<<g_MatrixMap->m_Size.x<<'x'<<g_MatrixMap->m_Size.y
        <<" groups="<<g_MatrixMap->m_GroupSize.x<<'x'<<g_MatrixMap->m_GroupSize.y;
    std::clog<<'\n';
}
void ValidateEntry(){
    auto* owner=cache_owner_detail::OriginalGameOwner();
    if(!owner || cache_owner_detail::Owner()!=owner || !owner->CpuReady() ||
       owner->OriginalStaticsReady() || g_MatrixHeap || g_Cache || g_CacheHeap || g_MatrixData)
        throw std::logic_error("original CGame::Init requires a fresh full-game resource owner");
}
void ObserveStatics(){
    auto* owner=cache_owner_detail::OriginalGameOwner();
    if(!owner || cache_owner_detail::Owner()!=owner)throw std::logic_error("original static init has no full-game owner");
    owner->statics_owned_=true;
}
void ValidateDeinit(){
    auto* owner=cache_owner_detail::OriginalGameOwner();
    if(!owner || cache_owner_detail::Owner()!=owner || !owner->CpuReady())
        throw std::logic_error("original CGame::Deinit requires its full-game resource owner");
}
}
void NativeInitRuntime::InitializeOriginalGame(const OriginalGameOptions& options){
    if(bool(options.window_provider)!=bool(options.device_provider))throw std::invalid_argument("original game requires paired borrowed providers");
    std::unique_ptr<resource_paths::NativeScope> input;
    if(options.resource_root)input=std::make_unique<resource_paths::NativeScope>(options.resource_root);
    BeginSession(options.module,options.window,true);
    try{
        original_skin_scope=std::make_unique<BorrowedSkinScope>(CSkinManager::GetSkin);
        if(options.window_provider){
            if(native_entry)BindLegacyProviders(*options.window_provider,*options.device_provider);
            else AttachDevice(*options.window_provider,*options.device_provider);
        }
        if(native_entry && native_sink)original_draw_scope=std::make_unique<NativeDrawScope>(*native_sink);
#ifdef _WIN32
        if(!native_entry && dynamic_cast<platform3g::D3D9Device*>(options.device_provider))
            original_draw_scope=std::make_unique<NativeDrawScope>(original_windows_sink);
#endif
        CGame::Init(options.module,options.window,options.map,options.seed,options.settings,
                    options.language,options.start_text,options.win_text,options.loss_text,options.planet);
        cache_owned_=g_Cache || g_CacheHeap;
        cache_owner_detail::OriginalGameInitialized()=true;
        resource_scope_=std::move(input);
    }catch(...){
        try{throw;}catch(const std::exception& error){std::clog<<"original Init exception: "<<error.what()<<'\n';}catch(...){}
        std::clog<<"original CGame::Init failure before cleanup: statics="<<statics_owned_
                 <<" heap="<<bool(g_MatrixHeap)<<" data="<<bool(g_MatrixData)
                 <<" cache="<<bool(g_Cache)<<'\n';
        // The original stack CLoadProgress has already unwound here.
        g_LoadProgress=nullptr;
        cache_owned_=g_Cache || g_CacheHeap;
        Deinit();
        throw;
    }
}
void NativeInitRuntime::InitializeNativeGame(const NativeGameOptions& options){
    if(native_entry)throw std::logic_error("recursive native game entry");
    const auto rect=options.window.ReadClientRect();
    const auto width=std::int64_t(rect.right)-rect.left,height=std::int64_t(rect.bottom)-rect.top;
    if(!options.window.Identity() || width<=0 || height<=0 || width>32768 || height>32768)
        throw std::invalid_argument("native drawable extent/identity invalid");
    std::unique_ptr<output_paths::NativeScope> output;
    if(options.output_root)output=std::make_unique<output_paths::NativeScope>(options.output_root);
    auto settings=SRobotsSettings::generate_default_settings();
    settings.FDirect3D=nullptr;settings.FD3DDevice=nullptr;
    settings.m_ResolutionX=static_cast<int>(width);settings.m_ResolutionY=static_cast<int>(height);
    OriginalGameOptions original;
    original.window=options.window.Identity();original.settings=&settings;original.map=options.map;original.seed=options.seed;
    original.language=options.language;original.start_text=options.start_text;original.win_text=options.win_text;
    original.loss_text=options.loss_text;original.planet=options.planet;
    original.resource_root=options.resource_root;
    original.window_provider=&options.window;original.device_provider=&options.device;
    struct EntryScope {
        EntryScope(NativeDrawSink* sink){native_entry=true;native_sink=sink;}
        ~EntryScope(){native_entry=false;native_sink=nullptr;}
    } scope(options.draw_sink);
    InitializeOriginalGame(original);
    output_scope_=std::move(output); // Keep original cache/output binding through frames and Deinit.
}
bool NativeInitRuntime::OriginalGameReady()const noexcept{
    return cache_owner_detail::OriginalGameOwner()==this && cache_owner_detail::OriginalGameInitialized();
}
void NativeInitRuntime::FinishOriginalGame(){
    if(cache_owner_detail::OriginalGameOwner()!=this)return;
    if(statics_owned_){
        FinishLoadProgress();
        CGame::Deinit(); // Original object teardown, HDelete heap and package release.
    }
    cache_owned_=g_Cache || g_CacheHeap;
    original_draw_scope.reset();
    original_skin_scope.reset();
    cache_owner_detail::OriginalGameInitialized()=false;
    cache_owner_detail::OriginalGameOwner()=nullptr;
}
}
