// SPDX-License-Identifier: GPL-2.0-or-later
// Production surface API/coordinator; low-level image/clear/transfer driver is
// explicitly a CPU model, NOT a GPU rasterizer or successful native Init.
#include "native_surfaces.hpp"
#include "offscreen_scope.hpp"
#include "native_frame_scope.hpp"
#include "native_map_queries.hpp"
#include "texture_residency.hpp"
#include "native_texture.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
namespace m=srhd_awa::matrixgame;namespace p=m::platform3g;namespace ns=m::native_surfaces;namespace os=m::offscreen;
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
struct Image final:ns::Resource {
 m::native_texture::Residency* arena{};m::native_texture::Residency::Range range{};std::vector<std::byte> bytes;
 Image(m::native_texture::Residency* a,unsigned size):arena(a),bytes(size){if(a)range=a->Allocate(size,256);}
 ~Image(){if(arena)arena->Release(range);}
};
struct Routing:p::Device,p::NativeMapQueries,p::NativeFrameProvider,ns::Provider {
 m::native_texture::Residency arena{16*1024*1024};std::shared_ptr<ns::Session> session=std::make_shared<ns::Session>();p::VideoViewport view{0,0,800,600,0,1};p::NativeFrameScope frame;
 bool oom{},vram{},bad_read{},bad_write{},bad_view{};unsigned reads{},writes{},clears{};
 Routing(){
  session->allocate=[this](const os::SurfaceDesc& d,bool){if(oom)throw std::bad_alloc();if(vram)throw m::native_texture::ResidencyExhausted();return std::make_shared<Image>(&arena,d.Width*d.Height*ns::PixelBytes(d.Format));};
  session->viewport=[this](const p::VideoViewport& v){if(bad_view)throw std::bad_alloc();view=v;};
  session->read=[this](const std::shared_ptr<ns::Surface>& s){if(bad_read)throw std::bad_alloc();if(frame)frame.RequirePresent();auto result=std::dynamic_pointer_cast<Image>(s->resource)->bytes;++reads;frame.Abort();return result;};
  session->write=[this](const std::shared_ptr<ns::Surface>& s,std::span<const std::byte> bytes){if(bad_write)throw std::bad_alloc();auto next=std::vector<std::byte>(bytes.begin(),bytes.end());std::dynamic_pointer_cast<Image>(s->resource)->bytes=std::move(next);++writes;};
  Primary(800,600);
 }
 void Primary(unsigned w,unsigned h){auto c=session->Make({21,0,0,w,h},false,false,true,std::make_shared<Image>(nullptr,w*h*4));auto d=session->Make({75,0,0,w,h},true,false,true,std::make_shared<Image>(nullptr,w*h*4));session->Presentation(std::move(c),std::move(d));}
 ~Routing(){session->Close();}
 std::shared_ptr<ns::Session> SurfaceSession()override{return session;}
 p::DeviceCaps ReadCaps()override{throw p::Unsupported("surface CPU model has no GPU caps");}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("surface CPU model has no GPU budget");}
 p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("surface CPU model has no VI");}
 void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
 p::DeviceResult SetVideoViewport(const p::VideoViewport& v)override{view=v;return 0;}
 p::VideoViewport NativeReadViewport()const override{return view;}
 p::DeviceResult NativeCooperativeLevel()const override{throw p::Unsupported("CPU model has no hardware queue");}
 p::DeviceResult NativeClear(std::span<const p::ClearRectangle> rectangles,unsigned flags,unsigned color,float depth,unsigned stencil)override{
  session->RequireDraw();auto& d=session->color->desc;auto plan=p::PlanNativeClear(view,d.Width,d.Height,rectangles,flags,depth,stencil);if(!frame)frame.Acquire();frame.RequireClear();
  if(flags&1){assert(d.Format==21);auto data=std::dynamic_pointer_cast<Image>(session->color->resource);for(auto r:plan)for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)std::memcpy(data->bytes.data()+(y*d.Width+x)*4,&color,4);}++clears;return 0;
 }
 p::DeviceResult NativeBeginScene()override{if(!frame)frame.Acquire();frame.BeginScene();return 0;}
 p::DeviceResult NativeEndScene()override{frame.EndScene();return 0;}
 p::DeviceResult NativePresent()override{throw p::Unsupported("surface CPU model cannot present");}
};
#ifndef MATRIXGAME_SURFACE_DRIVER_ONLY
int main(){Routing device;p::Current().ready=true;p::Current().device=&device;auto session=device.session;assert(ns::HandleCount()==0 && !session->DefaultResourcesLive());
 // The original D3D callers pass uninitialized output slots. Do not read them.
 IDirect3DSurface9* old;assert(os::GetTarget(0,&old)==0);IDirect3DSurface9* again;assert(os::GetTarget(0,&again)==0 && again==old);assert(ns::Find(old)->references==2);os::Release(again);assert(session->DefaultResourcesLive());assert(!device.frame);
 os::SurfaceDesc desc{};assert(os::Describe(old,&desc)==0 && desc.Width==800 && desc.Format==21);m::legacy_texture::LockedRect lock{};Reject([&]{os::Lock(old,&lock,nullptr,0);});
 auto initial=device.arena.Free();IDirect3DSurface9* target;assert(os::CreateTarget(128,64,21,0,0,true,&target,0)==0);auto retained=ns::Find(target);std::weak_ptr<ns::Resource> image=retained->resource;assert(device.arena.Free()==initial-128*64*4);
 p::VideoViewport before{7,9,100,50,0,1};os::SetViewport(&before);assert(os::SetTarget(0,target)==0 && device.view.Width==128 && device.view.X==0);session->RequireDraw();
 os::Clear(0,nullptr,1,0x80a319ef,1,0);os::BeginScene();Reject([&]{os::Lock(target,&lock,nullptr,0);});assert(!retained->locked);os::EndScene();
 assert(os::SetTarget(0,old)==0);os::SetViewport(&before);assert(os::Lock(target,&lock,nullptr,16)==0 && lock.Pitch==512);for(unsigned i=0;i<128*64;++i){unsigned value{};std::memcpy(&value,static_cast<std::byte*>(lock.pBits)+i*4,4);assert(value==0x80a319ef);}auto reads=device.reads;os::Unlock(target);assert(!device.frame && device.writes==0);
 device.bad_read=true;Reject([&]{os::Lock(target,&lock,nullptr,0);});assert(!retained->locked);device.bad_read=false;assert(os::Lock(target,&lock,nullptr,0)==0);os::Unlock(target);assert(device.writes==0 && device.reads==reads+1);
 assert(os::Lock(target,&lock,nullptr,0)==0);static_cast<std::byte*>(lock.pBits)[0]=std::byte{0x42};device.bad_write=true;Reject([&]{os::Unlock(target);});assert(retained->locked && std::dynamic_pointer_cast<Image>(retained->resource)->bytes[0]==std::byte{0xef});device.bad_write=false;os::Unlock(target);assert(device.writes==1);
 device.bad_view=true;Reject([&]{os::SetTarget(0,target);});assert(session->color==session->primary_color);device.bad_view=false;
 // A second controller keeps the same real resource after token release.
 os::Release(target);assert(!image.expired() && session->DefaultResourcesLive());retained.reset();assert(image.expired() && device.arena.Free()==initial);os::Release(old);assert(!session->DefaultResourcesLive());
 // Captured primary roles must rebind the replacement, not freed old images.
 auto lease=session->Capture();device.Primary(640,480);assert(lease->Restore()==0 && session->color==session->primary_color && device.view.Width==640);lease.reset();
 IDirect3DSurface9* large;assert(os::CreateTarget(1024,1024,21,0,0,true,&large,0)==0);assert(os::SetTarget(0,large)==0);Reject([&]{session->RequireDraw();});IDirect3DSurface9* z;assert(os::CreateDepth(1024,1024,75,0,0,true,&z,nullptr)==0);os::SetDepth(z);session->RequireDraw();
 auto captured=session->Capture();IDirect3DSurface9* primary;ns::Expose(session->primary_color,&primary);os::SetTarget(0,primary);os::Release(primary);os::SetDepth(nullptr);assert(captured->Restore()==0 && session->color==ns::Find(large) && session->depth==ns::Find(z));captured.reset();
 IDirect3DSurface9* back;ns::Expose(session->primary_color,&back);os::SetTarget(0,back);os::Release(back);session->Depth(session->primary_depth);os::Release(large);os::Release(z);assert(!session->DefaultResourcesLive());
 IDirect3DSurface9* failure=nullptr;device.oom=true;assert(os::CreateTarget(16,16,21,0,0,true,&failure,0)==m::native_texture::OutOfMemory && !failure);device.oom=false;device.vram=true;assert(os::CreateTarget(16,16,21,0,0,true,&failure,0)==m::native_texture::OutOfVideoMemory && !failure);device.vram=false;assert(os::CreateTarget(16,16,28,0,0,true,&failure,0)==0);os::Release(failure);failure=nullptr;Reject([&]{os::CreateTarget(16,16,21,2,0,true,&failure,0);});
 // The exact scope used by original CalcShadowTextureWOMat restores even if
 // draw/readback throws. No implicit present or fake successful scene follows.
 before={11,13,80,60,0.2f,0.8f};os::SetViewport(&before);try{
  os::TargetScope targets;os::SceneScope scene;os::SurfaceOwner temp;
  assert(os::CreateTarget(32,32,21,0,0,true,temp.put(),0)==0);os::SetTarget(0,temp);scene.Begin();throw std::bad_alloc();
 }catch(const std::bad_alloc&){}
 assert(session->color==session->primary_color && session->depth==session->primary_depth);assert(device.view.X==11 && device.view.MinZ==0.2f);if(device.frame)device.frame.RequirePresent();device.frame.Abort();assert(ns::HandleCount()==0 && !session->DefaultResourcesLive());
 // Device close drops GPU ownership even while a dead surface/CPU lock survives.
 os::CreateTarget(16,16,21,0,0,true,&failure,0);auto dying=ns::Find(failure);auto dead_lease=session->Capture();os::Lock(failure,&lock,nullptr,16);auto bytes=static_cast<std::byte*>(lock.pBits);session->Close();assert(!dying->resource && bytes[0]==std::byte{});assert(dead_lease->Restore()!=0);Reject([&]{os::Unlock(failure);});os::Release(failure);dying.reset();dead_lease.reset();assert(ns::HandleCount()==0 && device.arena.Free()==initial);
 p::Current().ready=false;p::Current().device=nullptr;
 std::puts("native surface get/create/bind/viewport/lock/refcount/retained-owner/OOM/read-write-failure/retry/lease/recreate/close PASS CPU_DRIVER_MODEL_ONLY NO_GPU NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN");
}

#endif
