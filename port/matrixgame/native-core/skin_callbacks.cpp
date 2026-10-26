// SPDX-License-Identifier: GPL-2.0-or-later
#include "skin_callbacks.hpp"
#include "VectorObjectCpu.hpp"
#include <unordered_map>
namespace srhd_awa::matrixgame {
namespace {thread_local SkinLoadScope* load_scope{};thread_local NativeDrawSink* draw_sink{};}
namespace {
thread_local BorrowedSkinScope* borrowed_scope{};
thread_local std::unordered_map<const void*,std::weak_ptr<const int>> borrowed_owners;
}
BorrowedSkinScope::BorrowedSkinScope(SkinGetter getter):previous_(borrowed_scope),getter_(getter){
    if(!getter)throw std::invalid_argument("borrowed skin callback missing");
    borrowed_scope=this;
}
BorrowedSkinScope::~BorrowedSkinScope(){borrowed_scope=previous_;}
bool BorrowedSkinProviderBound(SkinGetter getter) noexcept {
    for(auto* scope=borrowed_scope;scope;scope=scope->previous_)
        if(scope->getter_==getter)return true;
    return false;
}
void TrackBorrowedSkinOwner(const void* object,SkinGetter getter){
    if(!object)throw std::invalid_argument("borrowed skin object missing");
    for(auto* scope=borrowed_scope;scope;scope=scope->previous_)if(scope->getter_==getter){
        if(!scope->epoch_)scope->epoch_=std::make_shared<const int>(0);
        borrowed_owners.insert_or_assign(object,scope->epoch_);return;
    }
    throw std::logic_error("borrowed skin callback is not registered");
}
void CheckBorrowedSkinOwner(const void* object){
    const auto found=borrowed_owners.find(object);
    if(found!=borrowed_owners.end() && found->second.expired())
        throw std::logic_error("original skin manager lifetime expired; unload/reload required");
}
void ForgetBorrowedSkinOwner(const void* object) noexcept {borrowed_owners.erase(object);}
void InvalidateBorrowedSkins(SkinGetter getter) noexcept {
    for(auto* scope=borrowed_scope;scope;scope=scope->previous_)
        if(scope->getter_==getter)scope->epoch_.reset();
}
std::size_t BorrowedSkinLeaseCount() noexcept {return borrowed_owners.size();}
SkinLoadScope::SkinLoadScope(SkinProvider& provider):previous_(load_scope),provider_(provider){load_scope=this;}
SkinLoadScope::~SkinLoadScope(){load_scope=previous_;}
bool SkinProviderBound() noexcept {return load_scope!=nullptr;}
const SSkin* AcquireOwnedSkin(const wchar_t* name,std::uint32_t group) {
    if(!load_scope)throw std::logic_error("native skin provider not bound");
    auto skin=load_scope->provider_.AcquireSkin(name,group);
    if(!skin)throw std::logic_error("native skin provider returned no skin");
    if(!skin->m_Preload || !skin->m_SetupStages || !skin->m_SetupTex || !skin->m_SetupClear)
        throw std::logic_error("native skin provider returned incomplete callbacks");
    load_scope->pending_=std::move(skin);return load_scope->pending_.get();
}
std::shared_ptr<const SSkin> TakeOwnedSkin(const SSkin* skin) {
    if(!load_scope || load_scope->pending_.get()!=skin)
        throw std::logic_error("skin provider ownership mismatch");
    return std::move(load_scope->pending_);
}
NativeDrawScope::NativeDrawScope(NativeDrawSink& sink):previous_(draw_sink){draw_sink=&sink;}
NativeDrawScope::~NativeDrawScope(){draw_sink=previous_;}
NativeDrawSink& CurrentNativeDrawSink() {
    if(!draw_sink)throw std::logic_error("native draw sink not bound");
    return *draw_sink;
}
NativeDrawSink* InspectNativeDrawSink() noexcept {return draw_sink;}
}
