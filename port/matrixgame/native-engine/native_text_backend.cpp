// SPDX-License-Identifier: GPL-2.0-or-later
// CPU UI raster target plus the real native texture upload path. Opaque tokens
// are registered owners, never COM objects; no display render state is changed.
#ifndef _WIN32
#include "native_text.hpp"
#include "native_texture.hpp"
#include <algorithm>
#include <map>
namespace srhd_awa::matrixgame::text_backend {
namespace p=platform3g;namespace t=native_texture;
namespace {
struct Font {std::shared_ptr<const native::Data> data;std::shared_ptr<t::Session> device;};
struct Surface {std::shared_ptr<t::Texture> texture;unsigned level;};
struct Target {unsigned width,height;std::shared_ptr<Surface> surface;std::vector<std::byte> pixels;};
std::map<ID3DXFont*,std::shared_ptr<Font>> fonts;
std::map<IDirect3DSurface9*,std::shared_ptr<Surface>> surfaces;
std::map<ID3DXRenderToSurface*,std::unique_ptr<Target>> targets;
Target* active{};bool prepared{};
std::shared_ptr<t::Session> Device(){auto* provider=dynamic_cast<t::Provider*>(&p::ReadyDevice());if(!provider)throw p::Unsupported("native UI text requires native texture provider");auto s=provider->TextureSession();if(!s || !s->upload)throw p::Unsupported("native UI text device expired");return s;}
std::shared_ptr<const native::Data> Asset(unsigned id){auto device=Device();for(const auto& [token,font]:fonts)if(font->device==device && font->data->id==id)return font->data;auto* provider=dynamic_cast<native::Provider*>(&p::ReadyDevice());if(!provider)throw p::Unsupported("original UI font asset provider unavailable");auto d=native::Data::Load(provider->TextFontAsset(id));if(d->id!=id)throw std::invalid_argument("UI font asset identity");return d;}
Font& Find(ID3DXFont* token){auto i=fonts.find(token);if(i==fonts.end())throw std::invalid_argument("unregistered UI font");if(i->second->device!=Device())throw p::Unsupported("UI font device changed; rebuild original font cache");return *i->second;}
Target& Find(ID3DXRenderToSurface* token){auto i=targets.find(token);if(i==targets.end())throw std::invalid_argument("unregistered UI target");return *i->second;}
}
IDirect3DDevice9* DeviceIdentity(){return reinterpret_cast<IDirect3DDevice9*>(Device().get());}
void PrepareRangersFont(){(void)Asset(0);prepared=true;}
void ReleaseRangersFont()noexcept{prepared=false;}
p::DeviceResult CreateTextFont(IDirect3DDevice9* identity,std::wstring_view name,std::size_t size,ID3DXFont** out){
 if(!out)throw std::invalid_argument("UI font output");
 *out=nullptr;
 if(identity!=DeviceIdentity())throw std::invalid_argument("UI font device identity");
 unsigned id;if(name==L"Rangers" && size==10){if(!prepared)throw std::logic_error("original Rangers resource not prepared");id=0;}
 else if(name==L"Verdana" && size>=12 && size<=14)id=unsigned(size-11);else throw p::Unsupported("font outside original UI requests");
 auto font=std::make_shared<Font>();font->data=Asset(id);font->device=Device();auto token=reinterpret_cast<ID3DXFont*>(font.get());fonts.emplace(token,std::move(font));*out=token;return 0;
}
p::DeviceResult DescribeFont(ID3DXFont* font,FontDesc* out){if(!out)throw std::invalid_argument("null UI descriptor");out->Height=Find(font).data->size;return 0;}
bool MeasureSpace(ID3DXFont* font,const wchar_t* text,int count,Size* out){if(!text || !out || count<0 || count>65536)throw std::invalid_argument("UI measurement arguments");auto& d=*Find(font).data;std::int64_t width=0;for(int i=0;i<count;++i)width+=d.Lookup(text[i]).advance;if(width>INT32_MAX)throw std::length_error("UI measurement overflow");*out={int(width),int(d.height)};return true;}
int DrawFontText(ID3DXFont* font,const wchar_t* text,int count,Rect* rect,unsigned flags,unsigned color){
 if(!rect)throw std::invalid_argument("null UI bounds");auto& f=Find(font);
 if(flags&0x400)return native::Draw(*f.data,text,count,*rect,flags,color,{},0,0,0);
 if(!active || !active->surface || active->surface->texture->session!=f.device)throw std::logic_error("UI draw without matching target");
 return native::Draw(*f.data,text,count,*rect,flags,color,active->pixels,active->width,active->height,active->width*4);
}
void Release(ID3DXFont* font)noexcept{if(font && !fonts.erase(font))std::terminate();}
p::DeviceResult GetSurface(IDirect3DTexture9* texture,unsigned level,IDirect3DSurface9** out){
 if(!out)throw std::invalid_argument("UI surface output");*out=nullptr;auto t=t::Find(texture);auto& l=t->levels.at(level);
 if(t->format!=21 || t->session!=Device() || l.locked)throw std::invalid_argument("UI surface format/device/lock");
 auto s=std::make_shared<Surface>(Surface{std::move(t),level});auto token=reinterpret_cast<IDirect3DSurface9*>(s.get());surfaces.emplace(token,std::move(s));*out=token;return 0;
}
void Release(IDirect3DSurface9* surface)noexcept{if(surface && !surfaces.erase(surface))std::terminate();}
p::DeviceResult CreateRenderSurface(unsigned w,unsigned h,ID3DXRenderToSurface** out){
 if(!out)throw std::invalid_argument("UI target output");*out=nullptr;
 if(!w || !h || w>8192 || h>8192 || std::uint64_t(w)*h*4>64*1024*1024)throw std::invalid_argument("UI target extent");
 auto target=std::make_unique<Target>();target->width=w;target->height=h;target->pixels.resize(std::size_t(w)*h*4);auto token=reinterpret_cast<ID3DXRenderToSurface*>(target.get());targets.emplace(token,std::move(target));*out=token;return 0;
}
p::DeviceResult BeginScene(ID3DXRenderToSurface* renderer,IDirect3DSurface9* surface){
 auto& target=Find(renderer);auto i=surfaces.find(surface);if(active || i==surfaces.end())throw std::logic_error("UI nested scene/unknown surface");auto& s=*i->second;auto& l=s.texture->levels.at(s.level);
 if(s.texture->session!=Device() || l.locked || l.width!=target.width || l.height!=target.height)throw std::invalid_argument("UI target compatibility");
 target.pixels=l.bytes;target.surface=i->second;active=&target;return 0;
}
p::DeviceResult Clear(){if(!active)throw std::logic_error("UI clear outside scene");std::fill(active->pixels.begin(),active->pixels.end(),std::byte{});return 0;}
p::DeviceResult EndScene(ID3DXRenderToSurface* renderer,unsigned flags){
 auto& target=Find(renderer);if(active!=&target || !target.surface || flags)throw std::logic_error("UI end scene/flags");auto texture=target.surface->texture;
 if(texture->session!=Device())throw p::Unsupported("UI target device changed");auto candidate=*texture;candidate.levels.at(target.surface->level).bytes=target.pixels;candidate.dirty=true;
 t::ValidateUpload(candidate);auto image=texture->session->upload(candidate);if(!image)throw std::runtime_error("UI upload returned no native image");
 texture->levels.swap(candidate.levels);texture->image=std::move(image);texture->dirty=false;active=nullptr;target.surface.reset();return 0;
}
void Release(ID3DXRenderToSurface* renderer)noexcept{if(!renderer)return;auto i=targets.find(renderer);if(i==targets.end())std::terminate();if(active==i->second.get())active=nullptr;targets.erase(i);}
}
#endif
