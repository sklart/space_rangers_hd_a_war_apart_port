// SDK-free caller of the original bitmap serializer and actual SDK boundary.
#include "CBitmap.hpp"
#include "CBuf.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
void VerifyFormattedBitmapDevice() {
    namespace lt=srhd_awa::matrixgame::legacy_texture;
    namespace p=srhd_awa::matrixgame::platform3g;
    CBitmap bitmap;bitmap.CreateRGB(32,32);bitmap.Fill(Base::CPoint(0,0),Base::CPoint(32,32),0xff224466u);
    Base::CBuf bytes;bitmap.SaveInDDSUncompressed(bytes);
    for(int cycle=0;cycle<2;++cycle) {
        IDirect3DTexture9* texture=nullptr;
        p::CheckDeviceResult(lt::DecodeFormatted(bytes.Get(),bytes.Len(),6,0x31545844u,1,&texture),"original formatted DXT1 request");
        struct Owner {IDirect3DTexture9* value;~Owner(){lt::Release(value);}} owner{texture};
        assert(texture && lt::LevelCount(texture)==6);
        lt::SetPriority(texture,0xffffffffu);assert(lt::SetPriority(texture,0)==0xffffffffu);
        for(unsigned level=0;level<6;++level) {
            lt::LevelDesc desc{};p::CheckDeviceResult(lt::Describe(texture,level,&desc),"DXT1 mip descriptor");
            assert(desc.Format==0x31545844u && desc.Width==(32u>>level) && desc.Height==(32u>>level));
        }
        IDirect3DTexture9* failed=nullptr;
        const auto error=lt::DecodeFormatted(bytes.Get(),4,6,0x31545844u,1,&failed);
        assert(error<0 && !failed && lt::LevelCount(texture)==6);
        bool duplicate=false;
        try{lt::DecodeFormatted(bytes.Get(),bytes.Len(),6,0x31545844u,1,&texture);}
        catch(const std::invalid_argument&){duplicate=true;}
        assert(duplicate && texture==owner.value);
    }
    std::puts("original CBitmap DDS serialization -> actual D3DX DXT1/six mip creation, failure/output ownership and Release/recreate PASS; controlled bitmap fixture, no rendered pixels claimed");
}
