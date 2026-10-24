// Original CTexture on a real SDK device; no game-world substitution.
#include "Texture.hpp"
#include "unmanaged_texture_backend.hpp"
#include "CBitmap.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace srhd_awa::matrixgame;
namespace {
void CheckSystemPixels(CTexture& texture,std::uint32_t levels,std::uint32_t color) {
    namespace lt=legacy_texture;namespace ut=unmanaged_texture;
    assert(texture.DX() && lt::LevelCount(texture.DX())==levels);
    for(std::uint32_t level=0;level<levels;++level) {
        ut::ResourceDesc desc{};platform3g::CheckDeviceResult(ut::Describe(texture.DX(),level,&desc),"original system texture description");
        assert(desc.Pool==2 && desc.Format==21 && desc.Width==(8u>>level) && desc.Height==(8u>>level));
        lt::LockedRect locked{};platform3g::CheckDeviceResult(lt::Lock(texture.DX(),level,&locked,nullptr,16),"original system texture read lock");
        assert(locked.pBits && locked.Pitch>=int(desc.Width*4));
        for(std::uint32_t y=0;y<desc.Height;++y) {
            const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const unsigned char*>(locked.pBits)+y*locked.Pitch);
            for(std::uint32_t x=0;x<desc.Width;++x)assert(row[x]==color);
        }
        platform3g::CheckDeviceResult(lt::Unlock(texture.DX(),level),"original system texture read unlock");
    }
}
}
void VerifyOriginalUnmanagedTextureDevice() {
    namespace lt=legacy_texture;namespace ut=unmanaged_texture;
    const auto before=lt::RegistrySize();auto* managed=lt::CreateManaged();
    assert(managed && managed->IsTextureManaged() && !managed->IsLoaded());
    for(int kind=0;kind<2;++kind) {
        auto* texture=lt::CreateUnmanaged();assert(!texture->m_Cache && !texture->IsLoaded());
        const std::uint32_t color=kind?0xffffffffu:0xff123456u;
        const std::uint32_t levels=kind?4u:1u;
        if(!kind)texture->MakeSolidTexture(8,8,color);
        else {
            CBitmap bitmap;bitmap.CreateRGBA(8,8);std::memset(bitmap.Data(),0xff,8*8*4);
            texture->LoadFromBitmap(bitmap);
        }
        assert(texture->IsLoaded() && texture->Tex()==texture->DX() && lt::LevelCount(texture->DX())==levels);
        ut::ResourceDesc desc{};platform3g::CheckDeviceResult(ut::Describe(texture->DX(),0,&desc),"original default texture description");
        assert(desc.Pool==0 && desc.Usage==0 && desc.Width==8 && desc.Height==8);
        for(int pass=0;pass<2;++pass) {
            CBaseTexture::OnLostDevice();assert(texture->DX() && (texture->Flags()&TF_LOST) && !texture->Tex());
            CheckSystemPixels(*texture,levels,color);auto* backup=texture->DX();
            CBaseTexture::OnLostDevice();assert(texture->DX()==backup && !managed->IsLoaded());
            CBaseTexture::OnResetDevice();assert(texture->DX() && !(texture->Flags()&TF_LOST) && texture->Tex()==texture->DX());
            platform3g::CheckDeviceResult(ut::Describe(texture->DX(),0,&desc),"original restored texture description");
            assert(desc.Pool==0 && desc.Usage==0);
            auto* restored=texture->DX();CBaseTexture::OnResetDevice();assert(texture->DX()==restored);
        }
        texture->Unload();texture->Unload();texture->OnLostDevice();texture->OnResetDevice();
        assert(!texture->IsLoaded());CCache::Destroy(texture);assert(lt::RegistrySize()==before+1);
    }
    CCache::Destroy(managed);assert(lt::RegistrySize()==before);
    std::puts("original CTexture MakeSolidTexture + bitmap DDS decode, default/system/default loss-reset, all mip pixel copies, global dispatch skip-managed, Unload/Destroy/reentry PASS on actual HAL; real device Reset/Switch/minimap NOT EXECUTED");
}
