// Positive SDK test is called only by the separate real-HAL diagnostic driver.
#ifdef NDEBUG
#error Shadow GPU verification requires assertions
#endif
#include "VectorObject.hpp"
#include "ShadowProj.hpp"
#include "Texture.hpp"
#include "platform_3g.hpp"
#include "offscreen_backend.hpp"
#include "unmanaged_texture_backend.hpp"
#include "vector_picking.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
using namespace srhd_awa::matrixgame;
extern "C" bool MatrixGamePortableBoxProbe(const float* a){
    const D3DXVECTOR3 lo(a[0],a[1],a[2]),hi(a[3],a[4],a[5]);
    const D3DXVECTOR3 o(a[6],a[7],a[8]),d(a[9],a[10],a[11]);
    return vector_picking::BoxBoundProbe(&lo,&hi,&o,&d);
}
void VerifyMatrixGameBoxProbeSdk();
namespace {
std::size_t ReadAlpha(IDirect3DTexture9* texture,bool readback){
    namespace lt=legacy_texture;namespace os=offscreen;
    lt::LevelDesc desc{};platform3g::CheckDeviceResult(lt::Describe(texture,0,&desc),"shadow output description");
    assert(desc.Width==64 && desc.Height==64);
    const int pixel=desc.Format==28?1:(desc.Format==51 || desc.Format==29)?2:desc.Format==21?4:0;
    assert(pixel);
    IDirect3DTexture9* system=nullptr;IDirect3DSurface9 *source=nullptr,*destination=nullptr;
    struct Owner{IDirect3DTexture9*& t;IDirect3DSurface9 *&a,*&b;~Owner(){offscreen::Release(a);offscreen::Release(b);legacy_texture::Release(t);}} owner{system,source,destination};
    if(readback){
        platform3g::CheckDeviceResult(lt::Create(64,64,1,0,desc.Format,2,&system,0),"shadow readback system texture");
        platform3g::CheckDeviceResult(unmanaged_texture::Surface(texture,0,&source),"shadow source surface");
        platform3g::CheckDeviceResult(unmanaged_texture::Surface(system,0,&destination),"shadow readback surface");
        platform3g::CheckDeviceResult(os::Readback(source,destination),"shadow actual GPU readback");
    }
    lt::LockedRect lock{};auto* data=system?system:texture;
    platform3g::CheckDeviceResult(lt::Lock(data,0,&lock,nullptr,0),"shadow alpha pixels lock");
    assert(lock.Pitch>=64*pixel);std::size_t covered{};
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){
        const auto* row=static_cast<const std::uint8_t*>(lock.pBits)+y*lock.Pitch;
        if(row[x*pixel+pixel-1])++covered;
    }
    platform3g::CheckDeviceResult(lt::Unlock(data,0),"shadow alpha pixels unlock");
    assert(covered>0 && covered<64*64);return covered;
}
}
void VerifyOriginalShadowTextureDevice(const wchar_t* model,platform3g::Device& device,void(*bindings_check)()){
    VerifyMatrixGameBoxProbeSdk();
    namespace p=platform3g;namespace lt=legacy_texture;namespace os=offscreen;
    assert(p::Current().ready && !g_Cache && !g_CacheHeap && !lt::RegistrySize());
    CacheInit();struct CacheOwner{~CacheOwner(){CacheDeinit();}} cache;
    buffers::Context context;buffers::Scope scope(context);CVectorObject::StaticInit();
    auto* object=HNew(g_CacheHeap) CVectorObject;
    struct ModelOwner{CVectorObject*& value;~ModelOwner(){if(value)CCache::Destroy(value);}} object_owner{object};
    assert(bindings_check);object->m_Name=model;object->LoadSpecial(OLF_NO_TEX,nullptr,0);assert(object->IsLoaded());
    CVectorObjectAnim animation;animation.Init(object,nullptr);CVectorObjectAnim* objects[]{&animation};const int frames[]{0};
    auto state=device.CaptureState();assert(state);
    p::VideoViewport viewport{};p::CheckDeviceResult(os::GetViewport(&viewport),"shadow baseline viewport");
    const int old_x=g_ScreenX,old_y=g_ScreenY;const auto old_flags=g_Flags;
    struct Globals{int x,y;std::uint32_t flags;~Globals(){g_ScreenX=x;g_ScreenY=y;g_Flags=flags;}} globals{old_x,old_y,old_flags};
    g_ScreenX=viewport.Width;g_ScreenY=viewport.Height;g_Flags&=~GFLAG_STENCILAVAILABLE;
    IDirect3DSurface9 *target=nullptr,*depth=nullptr;
    p::CheckDeviceResult(os::GetTarget(0,&target),"shadow baseline target");
    p::CheckDeviceResult(os::GetDepth(&depth),"shadow baseline depth");assert(depth);
    struct Surfaces{IDirect3DSurface9 *a,*b;~Surfaces(){offscreen::Release(a);offscreen::Release(b);}} surfaces{target,depth};
    const auto registry=lt::RegistrySize();
    auto check_restored=[&]{
        p::VideoViewport after{};p::CheckDeviceResult(os::GetViewport(&after),"shadow restored viewport");
        assert(after.X==viewport.X && after.Y==viewport.Y && after.Width==viewport.Width && after.Height==viewport.Height && after.MinZ==viewport.MinZ && after.MaxZ==viewport.MaxZ);
        IDirect3DSurface9 *a=nullptr,*b=nullptr;
        p::CheckDeviceResult(os::GetTarget(0,&a),"shadow restored target");p::CheckDeviceResult(os::GetDepth(&b),"shadow restored depth");
        const bool same=a==target && b==depth;os::Release(a);os::Release(b);assert(same);bindings_check();
    };
    for(int repeat=0;repeat<2;++repeat){
        D3DXVECTOR3 light{1.f+repeat,2,-3};SProjData projection{};animation.CalcShadowProjMatrix(0,projection,light,6);
        std::printf("HAL original shadow begin managed repeat=%d\n",repeat);std::fflush(stdout);
        auto* result=CVectorObject::CalcShadowTexture(1,objects,frames,nullptr,projection,64,nullptr);
        assert(result && result->IsLoaded() && !result->m_Cache);const auto managed=ReadAlpha(result->DX(),false);
        check_restored();CCache::Destroy(result);assert(lt::RegistrySize()==registry);
        auto* update=lt::CreateUnmanaged();
        struct TextureOwner{CTexture* value;~TextureOwner(){CCache::Destroy(value);}} texture_owner{update};
        IDirect3DTexture9* identity=nullptr;
        for(int pass=0;pass<2;++pass){
            assert(!CVectorObject::CalcShadowTexture(1,objects,frames,nullptr,projection,64,nullptr,update));
            assert(update->IsLoaded());if(pass)assert(update->DX()==identity);identity=update->DX();
            const auto covered=ReadAlpha(update->DX(),true);check_restored();
            std::printf("HAL original shadow managed pixels=%zu update pass=%d pixels=%zu\n",managed,pass,covered);
        }
    }
    assert(lt::RegistrySize()==registry);animation.Clear();context.ClearBindings();CCache::Destroy(object);object=nullptr;
    assert(context.AllocatedBytes()==0);p::CheckDeviceResult(state->Restore(),"shadow outer SDK state restore");
    std::puts("original shadow textures on real VO: actual HAL GPU VB/IB upload/indexed draw, alpha-mask pixels, managed/new/update texture identity, target/depth/viewport restoration and cleanup/reentry PASS; Switch/world/battle NOT EXECUTED");
}
