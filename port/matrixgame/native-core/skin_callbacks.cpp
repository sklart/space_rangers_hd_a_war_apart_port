// SPDX-License-Identifier: GPL-2.0-or-later
#include "skin_callbacks.hpp"
#include "VectorObjectCpu.hpp"
namespace srhd_awa::matrixgame {
namespace {thread_local SkinLoadScope* load_scope{};thread_local NativeDrawSink* draw_sink{};}
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
