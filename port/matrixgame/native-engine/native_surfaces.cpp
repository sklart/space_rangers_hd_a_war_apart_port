// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_surfaces.hpp"
#include <algorithm>
#include <map>
#include <limits>
namespace srhd_awa::matrixgame::native_surfaces {
namespace p=platform3g;
namespace {std::map<IDirect3DSurface9*,std::shared_ptr<Surface>> handles;
void Live(const Surface& s){auto session=s.session.lock();if(!session || !session->live || (!s.System() && !s.resource))throw p::Unsupported("native surface device lifetime ended");if(s.texture && (!s.texture->session || s.texture->session!=session->textures || !s.texture->session->upload))throw p::Unsupported("texture surface device lifetime ended");if(s.presentation && session->primary_color.get()!=&s && session->primary_depth.get()!=&s)throw p::Unsupported("native presentation surface was replaced");}
void Own(const Session& session,const std::shared_ptr<Surface>& s,bool depth){if(!session.live || !s || s->session.lock().get()!=&session || s->depth!=depth)throw std::invalid_argument("native surface ownership/type");Live(*s);if(s->System() || (s->texture && !(s->texture->usage&1u)))throw p::Unsupported("surface is not a default-pool render target");if(s->locked || (s->texture && s->texture->levels.at(s->level).locked))throw std::logic_error("native surface is locked");}
}
unsigned PixelBytes(unsigned f){switch(f){case 28:return 1;case 21:case 22:case 75:case 77:return 4;case 23:case 80:return 2;default:throw p::Unsupported("native offscreen pixel format");}}
void Validate(offscreen::SurfaceDesc d,bool depth){
 if(!d.Width || !d.Height || d.Width>8192 || d.Height>8192 || d.MultiSampleType || d.MultiSampleQuality)throw std::invalid_argument("native offscreen dimensions/multisample");
 if(depth?(d.Format!=75 && d.Format!=77 && d.Format!=80):(d.Format!=21 && d.Format!=22 && d.Format!=23 && d.Format!=28))throw p::Unsupported("native offscreen render/depth format");
 if(std::uint64_t(d.Width)*d.Height*PixelBytes(d.Format)>64*1024*1024)throw std::length_error("native offscreen resource limit");
}
std::shared_ptr<Surface> Session::Make(offscreen::SurfaceDesc d,bool is_depth,bool lockable,bool primary,std::shared_ptr<Resource> resource){
 Validate(d,is_depth);if(!live || !resource || (is_depth && lockable))throw std::invalid_argument("native surface allocation/resource");
 auto s=std::make_shared<Surface>();s->session=shared_from_this();s->desc=d;s->depth=is_depth;s->lockable=lockable;s->presentation=primary;s->resource=std::move(resource);surfaces.push_back(s);return s;
}
std::shared_ptr<Surface> Session::Create(offscreen::SurfaceDesc d,bool is_depth,bool lockable){Validate(d,is_depth);if(!live || !allocate)throw p::Unsupported("native offscreen allocator unavailable");return Make(d,is_depth,lockable,false,allocate(d,is_depth));}
std::shared_ptr<Surface> Session::Import(std::shared_ptr<native_texture::Texture> t,unsigned level){
 if(!live || !t || t->session!=textures || !textures || !textures->upload)throw std::invalid_argument("texture surface device ownership/lifetime");
 const auto& l=t->levels.at(level);offscreen::SurfaceDesc d{t->format,0,0,l.width,l.height};Validate(d,false);
 for(const auto& w:surfaces)if(auto s=w.lock();s && s->texture==t && s->level==level)return s;
 auto s=std::make_shared<Surface>();s->session=shared_from_this();s->desc=d;s->texture=std::move(t);s->level=level;s->lockable=!(s->texture->usage&1u);
 if(!s->System()){if(!import_texture)throw p::Unsupported("texture surface GPU import unavailable");s->resource=import_texture(s->texture,level);if(!s->resource)throw std::runtime_error("texture surface import returned no image");}
 surfaces.push_back(s);return s;
}
void Session::Presentation(std::shared_ptr<Surface> c,std::shared_ptr<Surface> d)noexcept{primary_color=color=std::move(c);primary_depth=depth=std::move(d);}
void Session::Color(std::shared_ptr<Surface> next){Own(*this,next,false);if(!viewport)throw p::Unsupported("native offscreen viewport unavailable");viewport({0,0,next->desc.Width,next->desc.Height,0,1});color=std::move(next);}
void Session::Depth(std::shared_ptr<Surface> next){if(next)Own(*this,next,true);else if(!live)throw p::Unsupported("native surface session closed");depth=std::move(next);}
void Session::RequireDraw()const{Own(*this,color,false);if(depth){Own(*this,depth,true);if(depth->desc.Width<color->desc.Width || depth->desc.Height<color->desc.Height || depth->desc.MultiSampleType!=color->desc.MultiSampleType)throw std::invalid_argument("native color/depth extent mismatch");}}
bool Session::DefaultResourcesLive()const noexcept{for(const auto& w:surfaces)if(auto s=w.lock()){if(s->texture && s->texture->pool!=0)continue;if(!s->presentation || s->references)return true;const auto internal=1u+unsigned(s==color)+unsigned(s==depth)+unsigned(s==primary_color)+unsigned(s==primary_depth);if(s.use_count()>internal)return true;}return false;}
namespace {
std::array<const Surface*,4> BoundSurfaces(const Session& session)noexcept{
 return {session.color.get(),session.depth.get(),session.primary_color.get(),session.primary_depth.get()};
}
unsigned ResourceReferences(const Session& session,const Resource* resource)noexcept{
 const auto slots=BoundSurfaces(session);unsigned count=0;
 for(unsigned i=0;i<slots.size();++i){const auto* s=slots[i];if(!s || s->resource.get()!=resource)continue;
  bool repeated=false;for(unsigned j=0;j<i;++j)repeated|=slots[j]==s;if(!repeated)++count;}
 return count;
}
}
bool Session::ExternalDefaultResourcesLive()const noexcept{
 for(const auto& weak:surfaces)if(auto s=weak.lock()){
  if(s->texture && s->texture->pool!=0)continue;
  const auto internal=1u+unsigned(s==color)+unsigned(s==depth)+unsigned(s==primary_color)+unsigned(s==primary_depth);
  if(s->references || s.use_count()>internal)return true;
  if(s->resource && s->resource.use_count()>ResourceReferences(*this,s->resource.get()))return true;
 }
 return false;
}
void Session::AddDeviceTextureReferences(native_texture::DeviceReferences& references)const noexcept{
 const auto slots=BoundSurfaces(*this);
 for(unsigned i=0;i<slots.size();++i){const auto* s=slots[i];if(!s)continue;bool repeated=false;
  for(unsigned j=0;j<i;++j)repeated|=slots[j]==s;if(repeated)continue;
  references.Add(s->texture);
  if(s->resource)if(const auto* field=s->resource->ResetTextureReference())references.Add(*field);
 }
}
namespace {
class Bindings final:public p::DeviceStateLease {
 std::shared_ptr<Session> session;bool pc,pd,has_depth;std::shared_ptr<Surface> c,d;
public:
 explicit Bindings(std::shared_ptr<Session> s):session(std::move(s)),pc(session->color==session->primary_color),pd(session->depth==session->primary_depth),has_depth(bool(session->depth)){
  if(!session->live)throw p::Unsupported("capture closed surfaces");if(!pc)c=session->color;if(!pd)d=session->depth;
 }
 p::DeviceResult Restore()noexcept override{try{if(!session->live)return static_cast<std::int32_t>(0x88760868u);auto color=pc?session->primary_color:c;auto depth=has_depth?(pd?session->primary_depth:d):nullptr;Own(*session,color,false);if(depth)Own(*session,depth,true);
  // The combined device lease restores the exact viewport after these bindings.
  session->Color(color);session->Depth(depth);return 0;
 }catch(const std::bad_alloc&){return static_cast<std::int32_t>(0x8007000eu);}catch(...){return static_cast<std::int32_t>(0x8876086cu);}}
};
}
std::unique_ptr<p::DeviceStateLease> Session::Capture(){return std::make_unique<Bindings>(shared_from_this());}
void Session::Close()noexcept{live=false;import_texture={};textures.reset();allocate={};read={};write={};viewport={};for(auto& w:surfaces)if(auto s=w.lock())s->resource.reset();color.reset();depth.reset();primary_color.reset();primary_depth.reset();surfaces.clear();}
std::shared_ptr<Session> Active(){auto* provider=dynamic_cast<Provider*>(&p::ReadyDevice());if(!provider)throw p::Unsupported("native offscreen provider unavailable");auto s=provider->SurfaceSession();if(!s || !s->live)throw p::Unsupported("native offscreen device expired");return s;}
std::shared_ptr<Surface> Find(IDirect3DSurface9* token){auto i=handles.find(token);if(i==handles.end())throw std::invalid_argument("unregistered native surface token");return i->second;}
void Expose(const std::shared_ptr<Surface>& s,IDirect3DSurface9** out){if(!out)throw std::invalid_argument("null native surface output");*out=nullptr;if(!s)return;Live(*s);if(s->references==UINT32_MAX)throw std::length_error("native surface reference overflow");auto token=reinterpret_cast<IDirect3DSurface9*>(s.get());handles.try_emplace(token,s);++s->references;*out=token;}
void Release(IDirect3DSurface9* token)noexcept{if(!token)return;auto i=handles.find(token);if(i==handles.end() || !i->second->references)std::terminate();if(!--i->second->references)handles.erase(i);}
std::size_t HandleCount()noexcept{return handles.size();}
std::vector<std::byte> Read(const std::shared_ptr<Surface>& s){
 Live(*s);auto session=Active();if(session!=s->session.lock() || s->depth || s->locked || (s->texture && s->texture->levels.at(s->level).locked))throw std::invalid_argument("native surface read ownership/lock/type");
 auto bytes=s->System()?s->texture->levels.at(s->level).bytes:session->read?session->read(s):throw p::Unsupported("native surface readback unavailable");
 if(bytes.size()!=std::size_t(s->desc.Width)*PixelBytes(s->desc.Format)*s->desc.Height)throw std::runtime_error("native surface readback byte count");return bytes;
}
void Write(const std::shared_ptr<Surface>& s,std::span<const std::byte> bytes){
 Live(*s);auto session=Active();if(session!=s->session.lock() || s->depth || bytes.size()!=std::size_t(s->desc.Width)*PixelBytes(s->desc.Format)*s->desc.Height)throw std::invalid_argument("native surface write ownership/type/size");
 if(s->System()){auto next=std::vector<std::byte>(bytes.begin(),bytes.end());s->texture->levels.at(s->level).bytes.swap(next);s->texture->dirty=true;}
 else{if(!session->write)throw p::Unsupported("native surface writeback unavailable");session->write(s,bytes);}
}
void Copy(IDirect3DSurface9* source,IDirect3DSurface9* destination,bool render_readback){
 auto src=Find(source),dst=Find(destination);Live(*src);Live(*dst);
 if(src==dst || src->session.lock()!=dst->session.lock() || src->locked || dst->locked || src->depth || dst->depth ||
  (src->texture && src->texture->levels.at(src->level).locked) || (dst->texture && dst->texture->levels.at(dst->level).locked) ||
  src->desc.Width!=dst->desc.Width || src->desc.Height!=dst->desc.Height || src->desc.Format!=dst->desc.Format)throw p::Unsupported("native surface copy extent/format/device/lock compatibility");
 if(render_readback && (src->System() || (src->texture && !(src->texture->usage&1u)) || !dst->System()))throw std::invalid_argument("render target readback requires render source and system destination");
 auto bytes=Read(src);Write(dst,bytes);
}
p::DeviceResult Lock(IDirect3DSurface9* token,legacy_texture::LockedRect* out,unsigned flags){
 if(!out || (flags&~16u))throw std::invalid_argument("native surface lock flags/output");auto s=Find(token);Live(*s);if(s->depth || !s->lockable || s->locked)throw std::logic_error("native surface lockability/state");
 auto bytes=Read(s);const auto pitch=s->desc.Width*PixelBytes(s->desc.Format);auto before=flags&16?std::vector<std::byte>{}:bytes;
 s->pixels=std::move(bytes);s->before=std::move(before);s->read_only=flags&16;s->locked=true;if(s->texture)s->texture->levels.at(s->level).locked=true;*out={int(pitch),s->pixels.data()};return 0;
}
p::DeviceResult Unlock(IDirect3DSurface9* token){
 auto s=Find(token);Live(*s);if(!s->locked)throw std::logic_error("native surface unlock without lock");
 if(!s->read_only && s->pixels!=s->before)Write(s,s->pixels);
 if(s->texture)s->texture->levels.at(s->level).locked=false;s->locked=false;s->pixels.clear();s->before.clear();return 0;
}
}
