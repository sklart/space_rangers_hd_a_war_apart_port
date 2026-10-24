// SDK-free caller exercising real SDK offscreen resources, not a game world.
#include "offscreen_backend.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <source_location>
using namespace srhd_awa::matrixgame;
void VerifyOriginalOffscreenResources() {
    namespace p=platform3g;namespace o=offscreen;
    auto check=[](auto result,const std::source_location where=std::source_location::current()) {
        if(result!=p::DeviceSuccess)std::fprintf(stderr,"offscreen SDK call failed line=%u hr=%08x\n",where.line(),static_cast<unsigned>(result));
        assert(result==p::DeviceSuccess);
    };
    struct Surface {IDirect3DSurface9* value{};~Surface(){o::Release(value);}} original,original_depth;
    check(o::GetTarget(0,&original.value));
    std::puts("offscreen original target queried");
    const auto depth_result=o::GetDepth(&original_depth.value);
    assert(depth_result==0 || (depth_result<0 && !original_depth.value));
    p::VideoViewport viewport{};check(o::GetViewport(&viewport));
    std::printf("offscreen original depth result=%08x viewport=%ux%u\n",static_cast<unsigned>(depth_result),viewport.Width,viewport.Height);
    struct Restore {
        Surface& target;Surface& depth;p::VideoViewport viewport;
        ~Restore() {
            try {
                p::CheckDeviceResult(o::SetDepth(nullptr),"restore depth detach");
                p::CheckDeviceResult(o::SetTarget(0,target.value),"restore target");
                p::CheckDeviceResult(o::SetDepth(depth.value),"restore depth");
                p::CheckDeviceResult(o::SetViewport(&viewport),"restore viewport");
            } catch(...) {std::terminate();}
        }
    } restore{original,original_depth,viewport};
    for(std::uint32_t cycle=0;cycle<2;++cycle) {
        Surface target,depth;
        check(o::CreateTarget(64,64,21,0,0,true,&target.value,0));
        check(o::CreateDepth(64,64,80,0,0,true,&depth.value,nullptr));
        o::SurfaceDesc desc{};check(o::Describe(target.value,&desc));
        assert(desc.Width==64 && desc.Height==64 && desc.Format==21 && desc.MultiSampleType==0);
        check(o::Describe(depth.value,&desc));assert(desc.Width==64 && desc.Height==64 && desc.Format==80);
        std::printf("offscreen cycle=%u resources created/described\n",cycle);
        check(o::SetDepth(nullptr));check(o::SetTarget(0,target.value));check(o::SetDepth(depth.value));
        const p::VideoViewport small{0,0,64,64,0,1};check(o::SetViewport(&small));
        check(o::BeginScene());check(o::Clear(0,nullptr,3,0xff123456u,1.f,0));check(o::EndScene());
        std::puts("offscreen scene clear completed");
        check(o::SetDepth(nullptr));check(o::SetTarget(0,original.value));check(o::SetDepth(original_depth.value));
        check(o::SetViewport(&viewport));
        std::puts("offscreen original bindings restored");
        legacy_texture::LockedRect locked{};check(o::Lock(target.value,&locked,nullptr,0));
        assert(locked.pBits && locked.Pitch>=64*4);
        for(unsigned y=0;y<64;++y) {
            auto* row=reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(locked.pBits)+y*locked.Pitch);
            for(unsigned x=0;x<64;++x) {
                assert(row[x]==0xff123456u); // Actual HAL Clear output, before CPU writes.
                row[x]=0xa5000000u | (cycle<<20) | (y<<8) | x;
            }
        }
        check(o::Unlock(target.value));check(o::Lock(target.value,&locked,nullptr,16));
        std::printf("offscreen written/relocked pitch=%d\n",locked.Pitch);
        for(unsigned y=0;y<64;++y) {
            const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const unsigned char*>(locked.pBits)+y*locked.Pitch);
            for(unsigned x=0;x<64;++x)assert(row[x]==(0xa5000000u | (cycle<<20) | (y<<8) | x));
        }
        check(o::Unlock(target.value));
        std::puts("offscreen CPU pixel roundtrip verified");
        Surface restored,restored_depth;check(o::GetTarget(0,&restored.value));
        assert(restored.value==original.value);
        const auto restored_result=o::GetDepth(&restored_depth.value);
        assert((!depth_result && !restored_result && restored_depth.value==original_depth.value) ||
               (depth_result<0 && restored_result<0 && !restored_depth.value));
        p::VideoViewport actual{};check(o::GetViewport(&actual));
        assert(actual.X==viewport.X && actual.Y==viewport.Y && actual.Width==viewport.Width && actual.Height==viewport.Height && actual.MinZ==viewport.MinZ && actual.MaxZ==viewport.MaxZ);
    }
    std::puts("real D3D9 HAL offscreen target/depth create-bind-describe-scene-clear, actual Clear pixels, pitch-aware CPU lock roundtrip and exact target/depth/viewport restore/reentry PASS; robot portrait/draw/Switch NOT PROVEN");
}
