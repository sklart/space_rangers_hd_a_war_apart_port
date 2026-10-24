// Original border method against actual SDK texture storage; no draw simulation.
#include "Texture.hpp"
#include "Cache.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>

void VerifyOriginalRamkaDevice() {
    namespace lt=srhd_awa::matrixgame::legacy_texture;
    namespace p=srhd_awa::matrixgame::platform3g;
    assert(!g_Cache && !g_CacheHeap && lt::RegistrySize()==0);
    CacheInit();struct CacheOwner{~CacheOwner(){CacheDeinit();}} cache_owner;
    constexpr int width=8,height=7;
    constexpr std::uint32_t background=0x19283746u,border=0xfedcba98u;
    struct Region{int x,y,w,h;};
    const Region regions[]={{0,0,8,7},{2,1,4,3},{3,2,1,1},{1,3,5,1},{5,1,1,5}};
    for(int cycle=0;cycle<2;++cycle){
        auto* texture=lt::CreateManaged();
        struct Owner{CTextureManaged* value;~Owner(){CCache::Destroy(value);}} owner{texture};
        lt::LockedRect locked{};
        p::CheckDeviceResult(texture->CreateLock(static_cast<TexturePixelFormat>(21),width,height,1,locked),"Ramka texture creation");
        texture->UnlockRect();
        for(const auto& rect:regions){
            texture->LockRect(locked,0);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)
                reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(locked.pBits)+y*locked.Pitch)[x]=background;
            texture->UnlockRect();
            texture->Ramka(rect.x,rect.y,rect.w,rect.h,border);
            texture->LockRect(locked,16u);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                const bool inside=x>=rect.x && x<rect.x+rect.w && y>=rect.y && y<rect.y+rect.h;
                const bool edge=inside && (x==rect.x || x==rect.x+rect.w-1 || y==rect.y || y==rect.y+rect.h-1);
                const auto actual=reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(locked.pBits)+y*locked.Pitch)[x];
                assert(actual==(edge?border:background));
            }
            texture->UnlockRect();
        }
        // Original Ramka requires an in-bounds positive rectangle. D3D9 HAL
        // does not reliably reject negative rectangles; do not treat it as
        // a bounds-validation API or issue out-of-bounds writes in this test.
    }
    assert(lt::RegistrySize()==0);
    std::puts("original Ramka: actual D3D9 texture region lock, five valid border shapes, all 56 pixels per shape, unlock and Release/recreate PASS");
}
