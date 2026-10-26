// SPDX-License-Identifier: GPL-2.0-or-later
#define MATRIXGAME_SURFACE_DRIVER_ONLY
#include "test_matrixgame_native_surfaces.cpp"
#undef MATRIXGAME_SURFACE_DRIVER_ONLY
#include "minimap_lifetime.hpp"
namespace life=m::minimap_lifetime;
int main(){Routing device;p::Current().ready=true;p::Current().device=&device;auto session=device.session;auto color=session->color,depth=session->depth;p::VideoViewport before{7,9,80,60,0,1};os::SetViewport(&before);unsigned flags=17;
 try{
  life::FlagRollback state{flags,16};os::TargetScope target_restore;os::SceneScope scene;os::SurfaceOwner target;assert(os::CreateTarget(32,32,21,0,0,true,target.put(),0)==0);os::SetTarget(0,target);os::SetDepth(nullptr);flags&=~16u;flags|=8u;scene.Begin();throw std::bad_alloc();
 }catch(const std::bad_alloc&){}
 assert(session->color==color && session->depth==depth && device.view.X==7 && device.view.Y==9 && device.view.Width==80 && device.view.Height==60);assert(flags==25 && ns::HandleCount()==0);device.frame.RequirePresent();device.frame.Abort();
 {os::TargetScope targets;os::SurfaceOwner temp;os::CreateTarget(16,16,21,0,0,true,temp.put(),0);os::SetTarget(0,temp);device.bad_view=true;assert(targets.Restore()!=0);device.bad_view=false;assert(targets.Restore()==0);}
 assert(session->color==color && ns::HandleCount()==0);
 {os::SurfaceOwner surface;os::CreateTarget(16,16,21,0,0,true,surface.put(),0);auto retained=ns::Find(surface);try{m::legacy_texture::LockedRect rect{};os::SurfaceLockScope lock{surface,rect};static_cast<std::byte*>(rect.pBits)[0]=std::byte{42};throw std::runtime_error("original PNG writer failure");}catch(const std::runtime_error&){}assert(!retained->locked && device.writes==1);
  m::legacy_texture::LockedRect rect{};os::SurfaceLockScope lock{surface,rect};static_cast<std::byte*>(rect.pBits)[1]=std::byte{73};device.bad_write=true;Reject([&]{lock.Unlock();});assert(retained->locked);device.bad_write=false;lock.Unlock();assert(!retained->locked && device.writes==2);
 }
 bool stencil=true,projected=false;try{life::ValueRollback a{stencil};life::ValueRollback b{projected};stencil=false;projected=true;throw std::bad_alloc();}catch(const std::bad_alloc&){}assert(stencil && !projected);assert(ns::HandleCount()==0 && session->DefaultResourcesLive()); // Our verification owners still retain both primary surfaces.
 color.reset();depth.reset();assert(!session->DefaultResourcesLive());p::Current().ready=false;p::Current().device=nullptr;
 std::puts("production minimap target/depth/viewport/scene/flags/shadow-options/lock-scope/unlock-failure-retry PASS CPU_DRIVER_MODEL_ONLY NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN");
}
