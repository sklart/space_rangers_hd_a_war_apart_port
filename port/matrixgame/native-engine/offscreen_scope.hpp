// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "offscreen_backend.hpp"
namespace srhd_awa::matrixgame::offscreen {
class SurfaceOwner {
 IDirect3DSurface9* value_{};
public:
 SurfaceOwner()=default;SurfaceOwner(const SurfaceOwner&)=delete;
 ~SurfaceOwner(){Release(value_);}
 operator IDirect3DSurface9*()const noexcept{return value_;}
 IDirect3DSurface9** put(){reset();return &value_;}
 void reset()noexcept{Release(value_);value_=nullptr;}
};
class SurfaceLockScope {
 IDirect3DSurface9* surface_{};
public:
 SurfaceLockScope(IDirect3DSurface9*,legacy_texture::LockedRect&,std::uint32_t flags=0);
 ~SurfaceLockScope();SurfaceLockScope(const SurfaceLockScope&)=delete;
 void Unlock();
};
class TargetScope {
 SurfaceOwner color_,depth_;platform3g::VideoViewport viewport_;bool restored_{};
public:
 TargetScope();~TargetScope();TargetScope(const TargetScope&)=delete;
 platform3g::DeviceResult Restore()noexcept;
};
class SceneScope {
 bool open_{};
public:
 ~SceneScope();SceneScope()=default;SceneScope(const SceneScope&)=delete;
 void Begin();void End();
};
}
