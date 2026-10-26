// SPDX-License-Identifier: GPL-2.0-or-later
#include "offscreen_scope.hpp"
#include <cstdio>
namespace srhd_awa::matrixgame::offscreen {
namespace p=platform3g;
namespace {
template<class F>p::DeviceResult Attempt(F&& call)noexcept{try{return call();}catch(const p::DeviceError& e){return e.result;}catch(const std::bad_alloc&){return static_cast<std::int32_t>(0x8007000eu);}catch(...){return static_cast<std::int32_t>(0x88760868u);}}
void Report(const char* call,p::DeviceResult r)noexcept{if(r)std::fprintf(stderr,"%s cleanup failed: %08x\n",call,unsigned(r));}
}
SurfaceLockScope::SurfaceLockScope(IDirect3DSurface9* surface,legacy_texture::LockedRect& value,std::uint32_t flags){p::CheckDeviceResult(Lock(surface,&value,nullptr,flags),"offscreen LockRect");surface_=surface;}
void SurfaceLockScope::Unlock(){if(!surface_)throw std::logic_error("offscreen surface not locked");p::CheckDeviceResult(offscreen::Unlock(surface_),"offscreen UnlockRect");surface_=nullptr;}
SurfaceLockScope::~SurfaceLockScope(){if(surface_)Report("offscreen surface lock",Attempt([&]{return offscreen::Unlock(surface_);}));}
TargetScope::TargetScope(){p::CheckDeviceResult(GetTarget(0,color_.put()),"shadow capture target");auto depth=GetDepth(depth_.put());if(depth!=static_cast<std::int32_t>(0x88760866u))p::CheckDeviceResult(depth,"shadow capture depth");p::CheckDeviceResult(GetViewport(&viewport_),"shadow capture viewport");}
TargetScope::~TargetScope(){Report("shadow targets/viewport",Restore());}
p::DeviceResult TargetScope::Restore()noexcept{
 if(restored_)return 0;const auto color=Attempt([&]{return SetTarget(0,color_);});const auto depth=Attempt([&]{return SetDepth(depth_);});const auto view=Attempt([&]{return SetViewportValue(viewport_);});const auto result=color?color:depth?depth:view;restored_=!result;return result;
}
void SceneScope::Begin(){if(open_)throw std::logic_error("shadow scene already open");p::CheckDeviceResult(BeginScene(),"shadow BeginScene");open_=true;}
void SceneScope::End(){if(!open_)throw std::logic_error("shadow scene not open");p::CheckDeviceResult(EndScene(),"shadow EndScene");open_=false;}
SceneScope::~SceneScope(){if(open_)Report("shadow scene",Attempt([&]{return EndScene();}));}
}
