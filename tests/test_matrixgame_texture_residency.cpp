// SPDX-License-Identifier: GPL-2.0-or-later
// CPU resource lifetime/byte tests. The upload callback is an explicit CPU
// model; it never reports native GPU execution or successful native Init.
#include "native_texture.hpp"
#include "texture_residency.hpp"
#include "texture_asset.hpp"
#include "unmanaged_texture_backend.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
using namespace srhd_awa::matrixgame;
namespace nt=native_texture;
namespace lt=legacy_texture;
template<class F>void Reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}assert(bad);}
struct CpuImage final:nt::Image {
    nt::Residency& arena;nt::Residency::Range range;
    std::vector<nt::Level> levels;
    CpuImage(nt::Residency& a,const nt::Texture& t):arena(a),range(a.Allocate(256,256)),levels(t.levels){}
    ~CpuImage(){arena.Release(range);}
};
#ifndef _WIN32
// Routing fixture only. No caps/budget/Init/device success is supplied.
struct RoutingDevice final:platform3g::Device,nt::Provider {
    std::shared_ptr<nt::Session> session;
    explicit RoutingDevice(std::shared_ptr<nt::Session> s):session(std::move(s)){}
    std::shared_ptr<nt::Session> TextureSession()override{return session;}
    platform3g::DeviceCaps ReadCaps()override{throw platform3g::Unsupported("CPU routing fixture has no caps");}
    std::uint64_t AvailableTextureBytes()override{throw platform3g::Unsupported("CPU routing fixture has no GPU budget");}
    platform3g::GammaRamp ReadGamma(std::uint32_t)override{throw platform3g::Unsupported("CPU routing fixture has no gamma");}
    void RestoreGamma(std::uint32_t,const platform3g::GammaRamp&)noexcept override{std::terminate();}
};
#endif
void Allocations(){
    nt::Residency a(4096);auto first=a.Allocate(513,512),second=a.Allocate(1,1024);assert(first.offset==0 && second.offset==1024);
    assert(a.Free()==4096-514);auto free=a.Free();Reject([&]{a.Allocate(8192,256);});assert(a.Free()==free);
    a.Release(first);a.Release(second);assert(a.Free()==4096 && a.Largest()==4096);
    Reject([&]{a.Release(second);});Reject([&]{a.Allocate(1,3);});Reject([&]{a.Release({4095,2});});
    std::mt19937 rng(521);std::vector<nt::Residency::Range> live;
    for(unsigned i=0;i<4000;++i){if(!live.empty() && (rng()%3==0)){auto j=rng()%live.size();a.Release(live[j]);live.erase(live.begin()+j);}
        else {try{auto r=a.Allocate(1+rng()%300,1u<<(rng()%9));for(auto old:live)assert(std::uint64_t(r.offset)+r.size<=old.offset || std::uint64_t(old.offset)+old.size<=r.offset);live.push_back(r);}catch(const std::bad_alloc&){}}
        std::uint64_t used=0;for(auto r:live)used+=r.size;assert(a.Free()+used==4096);}
    for(auto r:live)a.Release(r);assert(a.Free()==4096 && a.Largest()==4096);
}
int main(int argc,char** argv){
    Allocations();nt::Residency arena(4096);auto session=std::make_shared<nt::Session>();bool fail=false;unsigned uploads=0;
    session->upload=[&](const nt::Texture& t)->std::shared_ptr<nt::Image>{if(fail)throw std::bad_alloc();++uploads;return std::make_shared<CpuImage>(arena,t);};
    auto count=nt::HandleCount();IDirect3DTexture9* token{};
    fail=true;Reject([&]{nt::Create(session,7,5,0,0,21,1,&token);});assert(!token && nt::HandleCount()==count && arena.Free()==4096);fail=false;
    nt::Create(session,7,5,0,0,21,1,&token);auto owner=nt::Find(token);assert(nt::Levels(token)==3 && arena.Free()==3840);
    // Two controls retain the same resource after the original owner releases
    // its handle. A recorded draw additionally retains the exact old image.
    auto control1=owner,control2=owner;auto frame=nt::Resident(owner);auto old=frame;
    lt::LockedRect lock{};nt::Lock(token,0,&lock,nullptr,0);assert(lock.Pitch==28);std::fill_n(static_cast<std::byte*>(lock.pBits),140,std::byte{0x91});
    Reject([&]{nt::Preload(token);});nt::Unlock(token,0);fail=true;Reject([&]{nt::Preload(token);});assert(owner->image==old && owner->dirty && arena.Free()==3840);fail=false;
    auto updated=nt::Resident(owner);assert(updated!=old && arena.Free()==3584);
    assert(std::dynamic_pointer_cast<CpuImage>(old)->levels[0].bytes[0]==std::byte{});
    assert(std::dynamic_pointer_cast<CpuImage>(updated)->levels[0].bytes[0]==std::byte{0x91});
    const auto calls=uploads;lt::TextureRect region{2,1,3,2};nt::Lock(token,0,&lock,&region,16);assert(static_cast<std::byte*>(lock.pBits)==owner->levels[0].bytes.data()+36);nt::Unlock(token,0);nt::Preload(token);assert(calls==uploads);
    Reject([&]{lt::TextureRect r{-1,0,1,1};nt::Lock(token,0,&lock,&r,0);});
    nt::Release(token);token=nullptr;owner.reset();control1.reset();assert(nt::Resident(control2)==updated);
    control2.reset();frame.reset();old.reset();updated.reset();assert(arena.Free()==4096 && nt::HandleCount()==count);
    unsigned formats=0;
    for(auto f:{21u,22u,23u,24u,25u,26u,28u,50u,51u,0x31545844u,0x33545844u,0x35545844u}){
        nt::Create(session,7,5,0,0,f,2,&token);auto t=nt::Find(token);assert(t->levels.size()==3);auto before=arena.Free();Reject([&]{nt::Preload(token);});assert(arena.Free()==before);
        for(unsigned i=0;i<3;++i){nt::Lock(token,i,&lock,nullptr,0);assert(lock.Pitch>0);auto& l=t->levels[i];std::fill(l.bytes.begin(),l.bytes.end(),std::byte(i+13));nt::Unlock(token,i);}
        if(f==0x31545844u){lt::TextureRect bad{1,0,5,4};Reject([&]{nt::Lock(token,0,&lock,&bad,0);});}
        nt::Release(token);token=nullptr;++formats;
    }
    IDirect3DTexture9* src{},*dst{};nt::Create(session,4,4,1,0,21,2,&src);nt::Create(session,4,4,1,0,21,0,&dst);
    nt::Lock(src,0,&lock,nullptr,0);std::fill_n(static_cast<std::byte*>(lock.pBits),64,std::byte{0xa3});nt::Unlock(src,0);
    auto prior=nt::Find(dst)->image;fail=true;Reject([&]{nt::Update(src,dst);});assert(nt::Find(dst)->image==prior && nt::Find(dst)->levels[0].bytes[0]==std::byte{});fail=false;
    nt::Update(src,dst);assert(nt::Find(dst)->levels[0].bytes[0]==std::byte{0xa3});prior.reset();nt::Release(src);nt::Release(dst);
    unsigned decoded=0;
    for(int arg=1;arg<argc;++arg){std::ifstream file(argv[arg],std::ios::binary);assert(file);std::vector<char> data{std::istreambuf_iterator<char>(file),{}};auto input=std::as_bytes(std::span(data));auto expected=TextureAsset::Decode(input);
        expected.PrepareMips(TextureMipPolicy::CompleteBox);auto before=data;
        nt::Decode(session,input.data(),input.size(),0,0,1,&token);auto t=nt::Find(token);assert(data==before && t->levels.size()==expected.levels.size());
        for(unsigned i=0;i<t->levels.size();++i){auto& l=t->levels[i];auto& e=expected.levels[i];assert(l.width==e.width && l.height==e.height && l.pitch==e.pitch && l.bytes.size()==e.bytes.size());
            if(expected.format!=TextureFormat::RGBA8)assert(l.bytes==e.bytes);
            else for(unsigned y=0;y<l.height;++y)for(unsigned x=0;x<l.width;++x){auto at=std::size_t(y)*l.pitch+x*4;assert(l.bytes[at]==e.bytes[at+2] && l.bytes[at+1]==e.bytes[at+1] && l.bytes[at+2]==e.bytes[at] && l.bytes[at+3]==e.bytes[at+3]);}}
        nt::Release(token);token=nullptr;++decoded;
    }
    assert(nt::HandleCount()==count && arena.Free()==4096);
#ifndef _WIN32
    RoutingDevice device(session);auto& state=platform3g::Current();assert(!state.ready);state.device=&device;state.ready=true;
    fail=true;assert(lt::Create(1,1,1,0,21,1,&token,0)==nt::OutOfMemory && !token);
    fail=false;auto old_callback=session->upload;session->upload=[](const nt::Texture&)->std::shared_ptr<nt::Image>{throw nt::ResidencyExhausted();};
    assert(lt::Create(1,1,1,0,21,1,&token,0)==nt::OutOfVideoMemory && !token);session->upload=old_callback;
    assert(lt::Create(2,2,1,0,21,1,&token,0)==0 && token);assert(lt::LevelCount(token)==1);
    lt::Lock(token,0,&lock,nullptr,0);std::fill_n(static_cast<std::byte*>(lock.pBits),16,std::byte{0x63});lt::Unlock(token,0);lt::Preload(token);lt::Release(token);token=nullptr;
    assert(nt::HandleCount()==count && arena.Free()==4096);state.ready=false;state.device=nullptr;
#endif
    nt::Create(session,1,1,1,0,21,1,&token);auto retained=nt::Find(token);session->Shutdown();assert(!retained->image && arena.Free()==4096);Reject([&]{nt::Resident(retained);});nt::Release(token);retained.reset();assert(nt::HandleCount()==count);
    std::printf("texture residency CPU PASS allocator_steps=4000 formats=%u decoded=%u held_owner_and_frame=preserved allocation_failure=preserved GPU_NOT_RUN\n",formats,decoded);
}
