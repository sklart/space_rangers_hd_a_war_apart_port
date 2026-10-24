// SDK-free caller of the real original managed texture and region lock.
#include "Texture.hpp"
#include "Cache.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
void VerifyOriginalPixelColorDevice() {
    namespace lt=srhd_awa::matrixgame::legacy_texture;
    namespace p=srhd_awa::matrixgame::platform3g;
    assert(!g_Cache && !g_CacheHeap && lt::RegistrySize()==0);
    CacheInit();struct CacheOwner{~CacheOwner(){CacheDeinit();}} cache_owner;
    const auto color=[](unsigned x,unsigned y){return 0x80000000u|((x+1)*0x110000u)|((y+1)*0x1100u)|(x+y*4+1);};
    for(int cycle=0;cycle<2;++cycle) {
        auto* texture=lt::CreateManaged();
        struct Owner{CTextureManaged* value;~Owner(){CCache::Destroy(value);}} owner{texture};
        lt::LockedRect locked{};
        p::CheckDeviceResult(texture->CreateLock(static_cast<TexturePixelFormat>(21),4,4,1,locked),"original pixel texture CreateLock");
        assert(locked.pBits && locked.Pitch>=16);
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)
            reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(locked.pBits)+y*locked.Pitch)[x]=color(x,y);
        texture->UnlockRect();
        for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)assert(texture->GetPixelColor(x,y)==color(x,y));
        assert(texture->GetPixelColor(-1,0)==0 && texture->GetPixelColor(0,-1)==0);
        assert(texture->GetPixelColor(4,0)==0 && texture->GetPixelColor(0,4)==0);
        assert(texture->GetPixelColor(3,3)==color(3,3)); // Bounds refusal must leave the texture unlocked.
        texture->LockRect(locked,0);
        reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(locked.pBits)+3*locked.Pitch)[3]=0x12345678u;
        texture->UnlockRect();assert(texture->GetPixelColor(3,3)==0x12345678u);
    }
    assert(lt::RegistrySize()==0);
    std::puts("original GetPixelColor: actual D3D9 region LockRect, 16 ARGB pixels, bounds, mutation, unlock and Release/recreate PASS");
}
