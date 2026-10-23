// Additional real game dependencies in the same bootstrap; no SDK type mixing.
#include "init_runtime.hpp"
#include "game_defaults.hpp"
#include MATRIXGAME_FULL_CONFIG_HEADER
#include MATRIXGAME_FULL_CAMERA_HEADER
#include MATRIXGAME_FULL_KONUS_HEADER
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace srhd_awa::matrixgame;
static_assert(sizeof(SKonusVertex)==20);
// Protected original cleanup is callable without constructing an effect or
// substituting its virtual gameplay methods. Statics come from original CPP.
struct KonusCleanupAccess:CMatrixEffectKonus {
    using CMatrixEffectKonus::MarkAllBuffersNoNeed;
    using CMatrixEffectKonus::StaticInit;
    static void SetBuffer(buffers::VertexBuffer* resource){m_VB=resource;}
    static bool Empty(){return m_VB==nullptr;}
};
void VerifyBootstrapGameCold() {
    assert(!g_Config.m_Cursors && g_Config.m_CursorsCnt==0);
    NativeInitRuntime absent;bool rejected=false;
    try{absent.InitializeGameDefaults();}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected && !absent.GameDefaultsReady());
}
void VerifyBootstrapGameDefaults(NativeInitRuntime& runtime) {
    assert(runtime.CpuReady() && !runtime.GameDefaultsReady());
    const auto was_gpu=runtime.GpuReady();
    // A refused new configuration must leave existing real allocated labels
    // untouched, and must not publish ownership to a failed initializer.
    auto* foreign=static_cast<SStringPair*>(HAlloc(sizeof(SStringPair),g_CacheHeap));
    new(foreign)SStringPair{};foreign->key=L"existing cursor";foreign->val=L"existing value";
    g_Config.m_Cursors=foreign;g_Config.m_CursorsCnt=1;
    bool rejected=false;try{runtime.InitializeGameDefaults();}catch(const std::logic_error&){rejected=true;}
    assert(rejected && !runtime.GameDefaultsReady() && g_Config.m_Cursors==foreign && foreign->key==L"existing cursor");
    ClearOriginalGameDefaults();
    runtime.InitializeGameDefaults();
    assert(runtime.GameDefaultsReady() && runtime.GpuReady()==was_gpu && g_MaxFPS==1000);
    assert(!g_Config.m_SoftwareCursor && g_Config.m_ObjTexturesGloss && g_Config.m_LandTexturesGloss);
    assert(g_Config.m_ShowStencilShadows && g_Config.m_ShowProjShadows && g_Config.m_VertexLight);
    assert(g_Config.m_CamParams[CAMERA_STRATEGY].m_CamDistMin==70 && g_Config.m_CamParams[CAMERA_STRATEGY].m_CamDistMax==250);
    assert(g_Config.m_CamParams[CAMERA_STRATEGY].m_CamAngleParam==0.4f && g_Config.m_CamParams[CAMERA_INROBOT].m_CamHeight==40);
    assert(g_Config.m_KeyActions[KA_ROTATE_LEFT]==0x24 && g_Config.m_KeyActions[KA_UNIT_FORWARD]==0x57);
    assert(g_Config.m_CaptureTimeErase==750 && g_Config.m_CaptureTimePaint==500 && g_Config.m_CaptureTimeRolback==1500);
    bool duplicate=false;try{runtime.InitializeGameDefaults();}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.GameDefaultsReady());
    // Real strings allocated on the real cache heap must be destroyed by the
    // original Clear during the shared owner's Deinit, not by this test.
    g_Config.m_Cursors=static_cast<SStringPair*>(HAlloc(2*sizeof(SStringPair),g_CacheHeap));
    for(int i=0;i<2;++i)new(g_Config.m_Cursors+i)SStringPair{};
    g_Config.m_CursorsCnt=2;g_Config.m_Cursors[0].key=L"first";g_Config.m_Cursors[1].val=L"second";
    auto& context=buffers::Current();const auto before=context.AllocatedBytes();
    KonusCleanupAccess::StaticInit();assert(KonusCleanupAccess::Empty());
    auto* resource=context.CreateVertex(96,vertex_layout::Position|vertex_layout::Texture1,buffers::Pool::Default,true);
    assert(resource && context.AllocatedBytes()==before+96);
    std::uint32_t* mapped=nullptr;NATIVE_BUF_LOCK_DISCARD(resource,&mapped);mapped[0]=0x12345678;NATIVE_BUF_UNLOCK(resource);
    KonusCleanupAccess::SetBuffer(resource);KonusCleanupAccess::MarkAllBuffersNoNeed();
    assert(KonusCleanupAccess::Empty() && context.AllocatedBytes()==before);
    KonusCleanupAccess::MarkAllBuffersNoNeed();assert(context.AllocatedBytes()==before);
    std::puts("original CMatrixConfig::SetDefaults EXECUTED + ownership refusal/retry; original Konus cleanup released real staging bytes; Clear pending owner Deinit");
}
void VerifyBootstrapProjection() {
    D3DXMATRIX world,view,projection;
    D3DXMatrixIdentity(&world);D3DXMatrixIdentity(&view);
    world._41=.25f;world._42=-.5f;world._43=2.f;
    view._41=-1.f;view._42=2.f;view._43=3.f;
    constexpr float fov=1.0471975512f,aspect=1280.f/720.f;
    D3DXMatrixPerspectiveFovLH(&projection,fov,aspect,1.f,100.f);
    const Viewport viewport{13,17,1280,720,.1f,.9f};
    const D3DXVECTOR3 point{2,1,10};D3DXVECTOR3 screen;
    D3DXVec3Project(&screen,&point,&viewport,&projection,&view,&world);
    const float focal=1.f/std::tan(fov/2);
    const float expected_x=13+(1+focal/aspect*1.25f/15.f)*640;
    const float expected_y=17+(1-focal*2.5f/15.f)*360;
    const float expected_z=.1f+.8f*(100.f/99.f-100.f/(99.f*15.f));
    assert(std::fabs(screen.x-expected_x)<1e-4f && std::fabs(screen.y-expected_y)<1e-4f && std::fabs(screen.z-expected_z)<1e-6f);
    const auto combined=world*view*projection;
    auto transformed=point;D3DXVec3TransformCoord(&transformed,&transformed,&combined);
    assert(std::fabs((transformed.x+1)*640+13-screen.x)<1e-4f && std::fabs(.1f+.8f*transformed.z-screen.z)<1e-6f);
    std::puts("full original Camera header COMPILED; DirectXMath world/view/projection + aliased transform regression PASS; camera object NOT CONSTRUCTED");
}
