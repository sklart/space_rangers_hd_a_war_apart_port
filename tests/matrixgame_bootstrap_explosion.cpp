#ifdef NDEBUG
#error Explosion/konus verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "Texture.hpp"
#include "Effects/MatrixEffectExplosion.hpp"
#include "Effects/MatrixEffectKonus.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace srhd_awa::matrixgame;
namespace {
// Observe protected original state without fields, virtual overrides or map.
struct SplashProbe final:CMatrixEffectKonusSplash {
    using CMatrixEffectKonusSplash::CMatrixEffectKonusSplash;
    using CMatrixEffectKonus::PrepareDX;
    static auto* Buffer(){return m_VB;}
    static int Owners(){return m_VB_ref;}
    float Height()const{return m_Height;}
    float Radius()const{return m_Radius;}
    float Z()const{return m_Pos.z;}
};
}
void VerifyBootstrapExplosion(NativeInitRuntime& runtime,const std::wstring& texture){
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    assert(!SplashProbe::Buffer() && !SplashProbe::Owners());
    const auto registry=legacy_texture::RegistrySize();
    // The common bootstrap already has a portable entry for this asset name.
    // The original cone also accepts a caller-owned genuine SDK texture.
    auto* tex=legacy_texture::CreateManaged();tex->m_Name=texture;
    struct Owner{CTextureManaged*& value;~Owner(){if(value)CCache::Destroy(value);}} owner{tex};
    const auto refs=tex->m_Ref;
    auto& context=buffers::Current();context.ClearBindings();const auto allocated=context.AllocatedBytes();
    for(int order=0;order<2;++order){
        auto* first=HNew(CMatrixEffect::m_Heap) SplashProbe({1,2,3},{1,0,0},10,20,0.25f,1000,true,tex);
        auto* second=HNew(CMatrixEffect::m_Heap) SplashProbe({1,2,3},{1,0,0},10,20,0.25f,1000,true,tex);
        assert(first->GetType()==EFFECT_SPLASH && first->Priority()==0);
        assert(tex->m_Ref==refs+2 && SplashProbe::Owners()==2);
        assert(first->Height()==0 && first->Radius()==0 && first->Z()==3);
        assert(SplashProbe::PrepareDX());auto* vb=SplashProbe::Buffer();assert(vb);
        assert(vb->Size()==(2+KONUS_NUM_SECTORS)*sizeof(SKonusVertex));
        assert(vb->Initialized(0,vb->Size()));assert(SplashProbe::PrepareDX() && vb==SplashProbe::Buffer());
        first->Takt(100);first->Takt(200);
        // Independent scalar oracle: the original 256-entry table rounds
        // the angle to the nearest sample. Do not call production TableSin.
        auto sine=[](double pi_factor){
            constexpr double pi=3.14159265358979323846;
            const auto index=static_cast<unsigned>(std::round(pi_factor*128))&255u;
            return std::sin(index*(2*pi/256));
        };
        assert(std::abs(first->Height()-20*sine(0.3*1.5))<2e-5);
        assert(std::abs(first->Radius()-10*sine(0.3*0.5))<2e-5);
        assert(std::abs(first->Z()-(3+10*sine(0.3)))<2e-5);
        bool refused=false;try{first->Draw();}catch(const platform3g::Unsupported&){refused=true;}
        assert(refused && !g_MatrixMap);
        auto* a=order?second:first;auto* z=order?first:second;
        a->Release();assert(tex->m_Ref==refs+1 && SplashProbe::Owners()==1 && SplashProbe::Buffer()==vb);
        z->Release();assert(tex->m_Ref==refs && !SplashProbe::Owners() && !SplashProbe::Buffer());
        assert(context.AllocatedBytes()==allocated);
    }
    // Access actual original exported presets, never reconstructed records.
    assert(ExplosionBuildingBoom.deb_cnt>0 && ExplosionBuildingBoom2.deb_cnt>0 && ExplosionObject.deb_cnt>0);
    CCache::Destroy(tex);tex=nullptr;assert(legacy_texture::RegistrySize()==registry);
    std::puts("full original Explosion/Konus integrated; original splash constructor/Takt/shared VB/texture refs/both release orders EXECUTED; GPU Draw refused; explosion world factory NOT EXECUTED");
}
