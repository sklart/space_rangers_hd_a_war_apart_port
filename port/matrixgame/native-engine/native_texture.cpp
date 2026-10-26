// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_texture.hpp"
#include "texture_asset.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <map>
namespace srhd_awa::matrixgame::native_texture {
namespace p=platform3g;
namespace {
std::map<void*,std::shared_ptr<Texture>> handles;
struct Format {unsigned bytes,block;};
Format Layout(std::uint32_t f){switch(f){
case 21:case 22:return {4,1}; // A8R8G8B8/X8R8G8B8
case 23:case 24:case 25:case 26:return {2,1}; // 565/X1555/1555/4444
case 28:case 50:return {1,1}; // A8/L8
case 51:return {2,1}; // A8L8
case 0x31545844:return {8,4};case 0x33545844:case 0x35545844:return {16,4};
default:throw p::Unsupported("native texture pixel format");}}
void Live(const Texture& t){if(!t.session || !t.session->upload)throw p::Unsupported("native texture device lifetime ended");}
std::shared_ptr<Session> Active(){auto* provider=dynamic_cast<Provider*>(&p::ReadyDevice());if(!provider)throw p::Unsupported("native texture provider unavailable");return provider->TextureSession();}
}
std::size_t HandleCount()noexcept{return handles.size();}
std::shared_ptr<Texture> Find(void* token){auto it=handles.find(token);if(it==handles.end())throw std::invalid_argument("unregistered native texture token");return it->second;}
void ValidateUpload(const Texture& t){
    if(t.levels.empty())throw std::invalid_argument("empty native texture upload");
    auto w=t.levels.front().width,h=t.levels.front().height;const auto f=Layout(t.format);
    if(!w || !h || w>8192 || h>8192 || t.levels.size()>std::bit_width(std::max(w,h)))throw std::invalid_argument("native upload extent/count");
    std::size_t total=0;
    for(const auto& l:t.levels){const auto pitch=((w+f.block-1)/f.block)*f.bytes,rows=(h+f.block-1)/f.block;
        const auto size=std::size_t(pitch)*rows;
        if(l.width!=w || l.height!=h || l.pitch!=pitch || l.rows!=rows || l.bytes.size()!=size || l.locked)throw std::invalid_argument("native upload mip layout");
        if(size>64*1024*1024-total)throw std::length_error("native upload byte limit");total+=size;w=std::max(1u,w/2);h=std::max(1u,h/2);
    }
}
void Session::Invalidate()noexcept {for(auto& w:textures)if(auto t=w.lock())t->image.reset();}
void Session::Shutdown()noexcept {upload={};Invalidate();textures.clear();}
void Release(void* token)noexcept {if(!token)return;auto it=handles.find(token);if(it==handles.end())std::terminate();handles.erase(it);}
std::shared_ptr<Image> Resident(const std::shared_ptr<Texture>& t){
    if(!t)throw std::invalid_argument("null native texture");Live(*t);
    if(t->pool==2)throw p::Unsupported("system memory texture cannot be sampled");
    for(const auto& l:t->levels)if(l.locked)throw std::logic_error("native texture still locked");
    if((t->usage&1u) && t->render_initialized && (t->dirty || !t->image))throw p::Unsupported("render texture contents require original recreation/restore");
    if(t->dirty || !t->image){ValidateUpload(*t);auto next=t->session->upload(*t);if(!next)throw std::runtime_error("native texture upload returned no image");t->image=std::move(next);t->dirty=false;if(t->usage&1u)t->render_initialized=true;}
    return t->image;
}
void Preload(void* token){(void)Resident(Find(token));}
std::uint32_t Priority(void* token,std::uint32_t value){auto t=Find(token);auto old=t->priority;t->priority=value;return old;}
std::uint32_t Levels(void* token){return std::uint32_t(Find(token)->levels.size());}
p::DeviceResult Describe(void* token,std::uint32_t level,legacy_texture::LevelDesc* out){if(!out)throw std::invalid_argument("null native texture desc");auto t=Find(token);auto& l=t->levels.at(level);*out={l.width,l.height,t->format};return 0;}
p::DeviceResult Lock(void* token,std::uint32_t level,legacy_texture::LockedRect* out,const legacy_texture::TextureRect* region,std::uint32_t flags){
    if(!out || (flags&~(16u|2048u|8192u|32768u)))throw p::Unsupported("native texture lock flags");
    auto t=Find(token);Live(*t);if(t->usage&1u)throw p::Unsupported("render texture LockRect requires readback to system memory");auto& l=t->levels.at(level);if(l.locked)throw std::logic_error("native texture double lock");
    const auto f=Layout(t->format);std::uint32_t x=0,y=0;
    if(region){if(region->left<0 || region->top<0 || region->right<=region->left || region->bottom<=region->top ||
        std::uint32_t(region->right)>l.width || std::uint32_t(region->bottom)>l.height)throw std::out_of_range("native texture lock rect");
        x=region->left;y=region->top;
        if(f.block>1 && (x%4 || y%4 || (std::uint32_t(region->right)!=l.width && region->right%4) || (std::uint32_t(region->bottom)!=l.height && region->bottom%4)))throw std::invalid_argument("compressed texture lock alignment");}
    if(flags&32768u)throw p::Unsupported("native texture NO_DIRTY_UPDATE lock");
    if((flags&8192u) && (!(t->usage&512u) || region || (flags&16u)))throw std::invalid_argument("native texture DISCARD lock");
    l.locked=true;l.read_only=flags&16u;*out={int(l.pitch),l.bytes.data()+std::size_t(y/f.block)*l.pitch+(x/f.block)*f.bytes};return 0;
}
p::DeviceResult Unlock(void* token,std::uint32_t level){auto t=Find(token);auto& l=t->levels.at(level);if(!l.locked)throw std::logic_error("native texture unlock without lock");l.locked=false;if(!l.read_only)t->dirty=true;return 0;}
p::DeviceResult Create(std::shared_ptr<Session> session,std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t pool,IDirect3DTexture9** out){
    if(!out)throw std::invalid_argument("null native texture output");*out=nullptr;
    if(!w || !h || w>8192 || h>8192 || pool>2)throw std::invalid_argument("native texture create extent/pool");
    if(usage&~513u)throw p::Unsupported("native depth/autogen texture creation not connected");
    if((usage&1u) && (usage!=1 || pool!=0 || levels!=1 || (format!=21 && format!=22 && format!=28)))throw p::Unsupported("native render texture format/mips/pool/usage");
    if(usage && pool==1)throw std::invalid_argument("dynamic managed texture");
    const auto f=Layout(format);const auto full=std::bit_width(std::max(w,h));if(!levels)levels=full;if(levels>full)throw std::invalid_argument("native texture mip count");
    auto t=std::make_shared<Texture>();t->session=std::move(session);Live(*t);t->format=format;t->pool=pool;t->usage=usage;
    std::size_t bytes=0;for(unsigned i=0;i<levels;++i){const auto pitch=((w+f.block-1)/f.block)*f.bytes,rows=(h+f.block-1)/f.block;
        if(std::uint64_t(pitch)*rows>64*1024*1024-bytes)throw std::length_error("native texture CPU byte limit");
        t->levels.push_back({w,h,pitch,rows,std::vector<std::byte>(std::size_t(pitch)*rows)});bytes+=std::size_t(pitch)*rows;w=std::max(1u,w/2);h=std::max(1u,h/2);}
    // Reserve and initialize the actual GPU image before publishing a handle.
    if(pool!=2)(void)Resident(t);
    t->session->textures.push_back(t);auto* key=t.get();handles.emplace(key,std::move(t));*out=reinterpret_cast<IDirect3DTexture9*>(key);return 0;
}
p::DeviceResult Create(std::uint32_t w,std::uint32_t h,std::uint32_t levels,std::uint32_t usage,std::uint32_t format,std::uint32_t pool,IDirect3DTexture9** out){return Create(Active(),w,h,levels,usage,format,pool,out);}
p::DeviceResult Decode(std::shared_ptr<Session> session,const void* input,std::size_t size,std::uint32_t levels,std::uint32_t format,std::uint32_t pool,IDirect3DTexture9** out){
    if(!out)throw std::invalid_argument("null native texture decode output");*out=nullptr;
    if(!input || !size)throw std::invalid_argument("native texture decode input");
    auto asset=TextureAsset::Decode({static_cast<const std::byte*>(input),size});
    if(format==0x31545844u && asset.format==TextureFormat::RGBA8){
        // Original formatted loader loads/encodes level zero with FILTER_NONE,
        // then generates BOX mips from the compressed previous level.
        asset.PrepareMips(TextureMipPolicy::OneLevel);
        asset.CompressBC1();
    }
    asset.PrepareMips(levels==1?TextureMipPolicy::OneLevel:TextureMipPolicy::CompleteBox,64*1024*1024,levels);
    std::uint32_t natural=asset.format==TextureFormat::RGBA8?21u:asset.format==TextureFormat::BC1?0x31545844u:asset.format==TextureFormat::BC2?0x33545844u:0x35545844u;
    if(!format)format=natural;
    if(format!=natural && (natural!=21 || (format!=22 && format!=23 && format!=25 && format!=26)))throw p::Unsupported("native texture formatted decode conversion");
    if(pool>2)throw p::Unsupported("native decoded texture pool");
    IDirect3DTexture9* candidate{};Create(std::move(session),asset.levels[0].width,asset.levels[0].height,std::uint32_t(asset.levels.size()),0,format,2,&candidate);
    struct Owner{IDirect3DTexture9* t;~Owner(){Release(t);}} owner{candidate};auto t=Find(candidate);
    for(unsigned i=0;i<asset.levels.size();++i){auto& src=asset.levels[i];auto& dst=t->levels[i];
        if(natural!=21){dst.bytes=src.bytes;continue;}
        for(unsigned y=0;y<src.height;++y)for(unsigned x=0;x<src.width;++x){auto* s=src.bytes.data()+std::size_t(y)*src.pitch+x*4;auto* d=dst.bytes.data()+std::size_t(y)*dst.pitch+x*Layout(format).bytes;
            const auto r=unsigned(s[0]),g=unsigned(s[1]),b=unsigned(s[2]),a=unsigned(s[3]);
            if(format==21 || format==22){d[0]=std::byte(b);d[1]=std::byte(g);d[2]=std::byte(r);d[3]=std::byte(format==22?255:a);}
            else{std::uint16_t v=format==23?((r>>3)<<11)|((g>>2)<<5)|(b>>3):format==25?((a>>7)<<15)|((r>>3)<<10)|((g>>3)<<5)|(b>>3):((a>>4)<<12)|((r>>4)<<8)|((g>>4)<<4)|(b>>4);std::memcpy(d,&v,2);}}
    }
    t->pool=pool;t->dirty=true;
    if(pool!=2)(void)Resident(t); // Complete original payload before GPU publication.
    *out=candidate;owner.t=nullptr;return 0;
}
p::DeviceResult Decode(const void* b,std::size_t n,std::uint32_t l,std::uint32_t f,std::uint32_t pool,IDirect3DTexture9** out){return Decode(Active(),b,n,l,f,pool,out);}
void Update(void* source,void* destination){auto src=Find(source),dst=Find(destination);Live(*src);Live(*dst);
    if(src->session!=dst->session || src->pool!=2 || dst->pool!=0 || src->format!=dst->format || src->levels.size()!=dst->levels.size())throw p::Unsupported("native UpdateTexture resource compatibility");
    for(unsigned i=0;i<src->levels.size();++i)if(src->levels[i].locked || dst->levels[i].locked || src->levels[i].width!=dst->levels[i].width || src->levels[i].height!=dst->levels[i].height)throw std::invalid_argument("native UpdateTexture levels");
    auto next=*dst;for(unsigned i=0;i<src->levels.size();++i)next.levels[i].bytes=src->levels[i].bytes;next.dirty=true;
    auto ready=dst->session->upload(next);if(!ready)throw std::runtime_error("native update returned no image");
    dst->levels.swap(next.levels);dst->image=std::move(ready);dst->dirty=false;
}
}
