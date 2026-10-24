// SPDX-License-Identifier: GPL-2.0-or-later
#include "init_runtime.hpp"
#include "cache_owner_state.hpp"
#include "MatrixGame.h"
#include "Texture.hpp"
#include "3g.hpp"
#include <iostream>
namespace srhd_awa::matrixgame {
namespace original_game {
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
    BeginSession(options.module,options.window,true);
    try{
        if(options.window_provider)AttachDevice(*options.window_provider,*options.device_provider);
        CGame::Init(options.module,options.window,options.map,options.seed,options.settings,
                    options.language,options.start_text,options.win_text,options.loss_text,options.planet);
        cache_owned_=g_Cache || g_CacheHeap;
        cache_owner_detail::OriginalGameInitialized()=true;
    }catch(...){
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
    cache_owner_detail::OriginalGameInitialized()=false;
    cache_owner_detail::OriginalGameOwner()=nullptr;
}
}
