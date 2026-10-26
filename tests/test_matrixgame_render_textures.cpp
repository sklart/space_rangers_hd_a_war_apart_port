// SPDX-License-Identifier: GPL-2.0-or-later
// Actual production resource coordinator with explicit CPU transfer driver.
// No rasterization/native Init/queue/Swapchain or Switch runtime is simulated as PASS.
#define MATRIXGAME_SURFACE_DRIVER_ONLY
#include "test_matrixgame_native_surfaces.cpp"
#undef MATRIXGAME_SURFACE_DRIVER_ONLY
#include "unmanaged_texture_backend.hpp"
namespace nt=m::native_texture;namespace ut=m::unmanaged_texture;
#include "../port/matrixgame/deko/alpha_target.hpp"
#include "../port/matrixgame/deko/material_combiner.hpp"
struct TexImage final:nt::Image{std::vector<std::byte> bytes;};
struct Imported final:ns::Resource{std::shared_ptr<nt::Texture> texture;unsigned level;};
int main(){Routing device;p::Current().ready=true;p::Current().device=&device;
 auto session=device.session;auto textures=std::make_shared<nt::Session>();session->textures=textures;bool upload_failure=false;unsigned uploads=0;
 textures->upload=[&](const nt::Texture& t){nt::ValidateUpload(t);if(upload_failure)throw std::bad_alloc();auto image=std::make_shared<TexImage>();image->bytes=t.levels[0].bytes;++uploads;return image;};
 session->import_texture=[](const std::shared_ptr<nt::Texture>& t,unsigned level){auto r=std::make_shared<Imported>();r->texture=t;r->level=level;(void)nt::Resident(t);return r;};
 session->read=[&](const std::shared_ptr<ns::Surface>& s){if(device.bad_read)throw std::bad_alloc();auto r=std::dynamic_pointer_cast<Imported>(s->resource);assert(r && r->level==0);return std::dynamic_pointer_cast<TexImage>(nt::Resident(r->texture))->bytes;};
 session->write=[&](const std::shared_ptr<ns::Surface>& s,std::span<const std::byte> bytes){if(device.bad_write)throw std::bad_alloc();auto r=std::dynamic_pointer_cast<Imported>(s->resource);auto image=std::dynamic_pointer_cast<TexImage>(r->texture->image);auto next=std::vector<std::byte>(bytes.begin(),bytes.end());image->bytes.swap(next);if(!(r->texture->usage&1u))r->texture->levels[r->level].bytes.assign(bytes.begin(),bytes.end());};
 IDirect3DTexture9* render{};upload_failure=true;Reject([&]{nt::Create(textures,32,16,1,1,28,0,&render);});assert(!render && nt::HandleCount()==0);upload_failure=false;
 assert(nt::Create(textures,32,16,1,1,28,0,&render)==0);auto t=nt::Find(render);assert(t->render_initialized && uploads==1);auto image=nt::Resident(t);auto original_image=std::dynamic_pointer_cast<TexImage>(image);for(unsigned i=0;i<original_image->bytes.size();++i)original_image->bytes[i]=std::byte(i%256); // CPU driver transfer payload, not rendered glyphs/shadows.
 IDirect3DSurface9* surface;assert(ut::Surface(render,0,&surface)==0);IDirect3DSurface9* again;assert(ut::Surface(render,0,&again)==0 && again==surface);os::Release(again);assert(session->DefaultResourcesLive());os::SetTarget(0,surface);session->RequireDraw();m::legacy_texture::LockedRect lock{};Reject([&]{nt::Lock(render,0,&lock,nullptr,0);});Reject([&]{os::Lock(surface,&lock,nullptr,0);});
 IDirect3DTexture9* sys{};assert(nt::Create(textures,32,16,1,0,28,2,&sys)==0);IDirect3DSurface9* system;ut::Surface(sys,0,&system);Reject([&]{os::SetTarget(0,system);});
 device.bad_read=true;auto before=nt::Find(sys)->levels[0].bytes;assert(os::Readback(surface,system)==nt::OutOfMemory);assert(nt::Find(sys)->levels[0].bytes==before);device.bad_read=false;assert(os::Readback(surface,system)==0);assert(nt::Find(sys)->levels[0].bytes==original_image->bytes);
 assert(os::Lock(system,&lock,nullptr,16)==0 && lock.Pitch==32);assert(std::memcmp(lock.pBits,original_image->bytes.data(),512)==0);Reject([&]{nt::Lock(sys,0,&lock,nullptr,0);});Reject([&]{ns::Copy(surface,system);});os::Unlock(system);
 assert(nt::Lock(sys,0,&lock,nullptr,0)==0);Reject([&]{os::Lock(system,&lock,nullptr,0);});nt::Unlock(sys,0);
 // Surface keeps the texture/GPU image alive after legacy texture owner release.
 nt::Release(render);assert(nt::HandleCount()==1 && nt::Resident(t)==image);nt::Release(sys);assert(nt::HandleCount()==0);assert(os::Lock(system,&lock,nullptr,16)==0);os::Unlock(system);
 // Cross-device, alias, mismatched dimensions and format must never report success.
 Reject([&]{ns::Copy(surface,surface);});auto foreign=std::make_shared<ns::Session>();foreign->textures=std::make_shared<nt::Session>();Reject([&]{foreign->Import(t,0);});
 IDirect3DTexture9* bad{};Reject([&]{nt::Create(textures,32,16,1,1,28,1,&bad);});Reject([&]{nt::Create(textures,32,16,2,1,28,0,&bad);});Reject([&]{nt::Create(textures,32,16,1,513,28,0,&bad);});assert(!bad);
 session->Color(session->primary_color);os::Release(surface);os::Release(system);assert(ns::HandleCount()==0);assert(textures->DefaultResourcesLive());
 auto count=uploads;textures->Invalidate();Reject([&]{nt::Resident(t);});assert(uploads==count); // Never silently replace GPU-rendered contents with CPU zeroes.
 t.reset();image.reset();original_image.reset();assert(!textures->DefaultResourcesLive() && !session->DefaultResourcesLive());
 for(unsigned format:{21u,22u}){IDirect3DTexture9* value;assert(nt::Create(textures,8,8,1,1,format,0,&value)==0);IDirect3DTexture9* staged{};nt::Create(textures,8,8,1,0,format,2,&staged);nt::Find(staged)->levels[0].bytes[9]=std::byte{77};auto prior=nt::Resident(nt::Find(value));nt::Update(staged,value);assert(nt::Resident(nt::Find(value))!=prior && std::dynamic_pointer_cast<TexImage>(nt::Resident(nt::Find(value)))->bytes[9]==std::byte{77});nt::Release(staged);nt::Release(value);}
 // Managed surface writeback shares the original level lock and CPU backing.
 IDirect3DTexture9* managed{};nt::Create(textures,8,8,1,0,21,1,&managed);IDirect3DSurface9* ms;ut::Surface(managed,0,&ms);assert(!session->DefaultResourcesLive());os::Lock(ms,&lock,nullptr,0);static_cast<std::byte*>(lock.pBits)[7]=std::byte{91};device.bad_write=true;Reject([&]{os::Unlock(ms);});assert(nt::Find(managed)->levels[0].locked);device.bad_write=false;os::Unlock(ms);assert(nt::Find(managed)->levels[0].bytes[7]==std::byte{91});os::Release(ms);nt::Release(managed);
 for(unsigned mask=0;mask<16;++mask){m::MeshRasterState r;r.color_write=mask;r.source=m::RasterBlendFactor::DestinationAlpha;r.destination=m::RasterBlendFactor::InverseDestinationAlpha;auto mapped=m::AlphaTargetRaster(r);assert(mapped.color_write==((mask&8)?1u:0u));assert(mapped.source==m::RasterBlendFactor::DestinationColor && mapped.destination==m::RasterBlendFactor::InverseDestinationColor);}
 assert(m::AlphaTargetFactor(m::RasterBlendFactor::SourceAlphaSaturate)==m::RasterBlendFactor::One);
 session->Close();textures->Shutdown();p::Current().ready=false;p::Current().device=nullptr;assert(nt::HandleCount()==0 && ns::HandleCount()==0);
 std::puts("render texture A8/BGRA/XRGB creation/import/canonical-refcount/retained-owner/readback/lock-exclusion/OOM/failure-atomicity/invalidation/raster PASS CPU_DRIVER_MODEL_ONLY NO_GPU NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN");return 0;
}
