// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "offscreen_backend.hpp"
#include "native_texture.hpp"
#include <functional>
#include <memory>
#include <span>
#include <vector>
namespace srhd_awa::matrixgame::native_surfaces {
struct Resource {
 virtual ~Resource()=default;
 // A backend may own one additional texture reference behind a surface.
 // Returning the actual field permits identity/owner checks without copying it.
 virtual const std::shared_ptr<native_texture::Texture>* ResetTextureReference()const noexcept{return nullptr;}
};
struct Session;
struct Surface {
 std::weak_ptr<Session> session;
 offscreen::SurfaceDesc desc;
 bool depth{},lockable{},presentation{},locked{},read_only{};
 std::uint32_t references{};
 std::shared_ptr<Resource> resource;
 std::shared_ptr<native_texture::Texture> texture;
 unsigned level{};
 bool System()const noexcept{return texture && texture->pool==2;}
 std::vector<std::byte> pixels,before;
};
struct Session:std::enable_shared_from_this<Session> {
 bool live{true};
 std::function<std::shared_ptr<Resource>(const offscreen::SurfaceDesc&,bool)> allocate;
 std::function<std::vector<std::byte>(const std::shared_ptr<Surface>&)> read;
 std::function<void(const std::shared_ptr<Surface>&,std::span<const std::byte>)> write;
 std::function<void(const platform3g::VideoViewport&)> viewport;
 std::shared_ptr<native_texture::Session> textures;
 std::function<std::shared_ptr<Resource>(const std::shared_ptr<native_texture::Texture>&,unsigned)> import_texture;
 std::shared_ptr<Surface> color,depth,primary_color,primary_depth;
 std::vector<std::weak_ptr<Surface>> surfaces;
 std::shared_ptr<Surface> Make(offscreen::SurfaceDesc,bool,bool,bool,std::shared_ptr<Resource>);
 std::shared_ptr<Surface> Create(offscreen::SurfaceDesc,bool,bool);
 std::shared_ptr<Surface> Import(std::shared_ptr<native_texture::Texture>,unsigned);
 void Presentation(std::shared_ptr<Surface>,std::shared_ptr<Surface>)noexcept;
 void Color(std::shared_ptr<Surface>);
 void Depth(std::shared_ptr<Surface>);
 void RequireDraw()const;
 bool DefaultResourcesLive()const noexcept;
 bool ExternalDefaultResourcesLive()const noexcept;
 void AddDeviceTextureReferences(native_texture::DeviceReferences&)const noexcept;
 std::unique_ptr<platform3g::DeviceStateLease> Capture();
 void Close()noexcept;
};
class Provider {public:virtual ~Provider()=default;virtual std::shared_ptr<Session> SurfaceSession()=0;};
unsigned PixelBytes(unsigned);
void Validate(offscreen::SurfaceDesc,bool);
std::shared_ptr<Session> Active();
std::shared_ptr<Surface> Find(IDirect3DSurface9*);
void Expose(const std::shared_ptr<Surface>&,IDirect3DSurface9**);
void Release(IDirect3DSurface9*)noexcept;
std::size_t HandleCount()noexcept;
platform3g::DeviceResult Lock(IDirect3DSurface9*,legacy_texture::LockedRect*,unsigned);
platform3g::DeviceResult Unlock(IDirect3DSurface9*);
std::vector<std::byte> Read(const std::shared_ptr<Surface>&);
void Write(const std::shared_ptr<Surface>&,std::span<const std::byte>);
void Copy(IDirect3DSurface9* source,IDirect3DSurface9* destination,bool render_readback=false);
}
