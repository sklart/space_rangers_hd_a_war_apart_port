// Additional real game dependencies in the same bootstrap; no SDK type mixing.
#include "init_runtime.hpp"
#include "game_defaults.hpp"
#include MATRIXGAME_FULL_CONFIG_HEADER
#include MATRIXGAME_FULL_CAMERA_HEADER
#include MATRIXGAME_FULL_KONUS_HEADER
#include MATRIXGAME_FULL_CURSOR_HEADER
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include "MatrixGame.h"
using namespace srhd_awa::matrixgame;
void VerifyBootstrapVideo(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,bool negotiated) {
    namespace p=platform3g;
    auto settings=SRobotsSettings::generate_default_settings();
    if(!negotiated) {
        bool rejected=false;try{runtime.ApplyVideoSettings(settings);}catch(const p::Unsupported&){rejected=true;}
        assert(rejected && runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
        std::puts("original CGame video unavailable without actual window/device; no synthetic display");return;
    }
    window.Show(p::Visibility::Normal);window.Update();
    const auto creation=device.ReadCreationParameters();
    const auto mode=device.ReadAdapterDisplayMode(creation.AdapterOrdinal);
    std::printf("video actual creation: adapter=%u type=%u flags=%u focus=%p bound=%p mode=%ux%u/%u\n",creation.AdapterOrdinal,creation.DeviceType,creation.BehaviorFlags,creation.FocusWindow,window.Identity(),mode.Width,mode.Height,mode.RefreshRate);
    // NULLREF reports a null focus window even when CreateDevice received a
    // HWND. The SDK-only helper checks every returned field against raw SDK.
    assert(window.Identity() && mode.Width>0 && mode.Height>0);
    const auto client=window.ReadClientRect(),outer=window.ReadWindowRect();
    settings.m_ResolutionX=client.right;settings.m_ResolutionY=client.bottom;
    settings.m_BPP=(mode.Format==p::PixelFormat::Xrgb8 || mode.Format==p::PixelFormat::Argb8)?32:16;
    settings.m_RefreshRate=0;settings.m_FSAASamples=0;
    runtime.ApplySamplerSettings(settings);
    const auto flags=p::Current().flags;const auto presentation=p::Current().presentation;
    const auto world=device.ReadWorldTransform();const auto leases=device.OutstandingStateLeases();
    for(int attempt=0;attempt<2;++attempt) {
        bool rejected=false;
        try{runtime.ApplyVideoSettings(settings);}
        catch(const p::Unsupported& e){
            rejected=true;const std::string message=e.what();
            assert(message=="window resize requires reset-safe presentation ownership");
            std::printf("original CGame::ApplyVideoParams EXECUTED actual adapter=%u type=%u mode=%ux%u/%u client=%dx%d -> %s\n",creation.AdapterOrdinal,creation.DeviceType,mode.Width,mode.Height,mode.RefreshRate,client.right,client.bottom,e.what());
        }
        assert(rejected && runtime.GpuReady() && runtime.GameDefaultsReady() && !g_MatrixMap);
        assert(p::Current().flags==flags && p::Current().presentation==presentation);
        assert(window.ReadClientRect()==client && window.ReadWindowRect()==outer);
        assert(device.ReadWorldTransform()==world && device.OutstandingStateLeases()==leases);
    }
    settings.m_FSAASamples=17;
    bool invalid=false;try{runtime.ApplyVideoSettings(settings);}catch(const p::Unsupported&){assert(false);}catch(...){invalid=true;}
    assert(invalid && p::Current().flags==flags && p::Current().presentation==presentation);
    // Real refresh mismatch selects the original fullscreen path. It must
    // reject before changing the caller's style or resetting its device.
    settings.m_FSAASamples=0;settings.m_RefreshRate=mode.RefreshRate+1;
    bool fullscreen_denied=false;
    try{runtime.ApplyVideoSettings(settings);}catch(const p::Unsupported& e){fullscreen_denied=std::string(e.what())=="fullscreen style requires reset-safe presentation ownership";}
    assert(fullscreen_denied && window.ReadWindowRect()==outer && window.ReadClientRect()==client);
    assert(p::Current().flags==flags && p::Current().presentation==presentation && !g_MatrixMap);
    std::puts("CGame video validation/window/fullscreen refusal, shared-state rollback and retry PASS; Reset/map/material/light NOT EXECUTED");
}
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
    assert(!g_MatrixHeap);
    {
        Base::CHeap foreign_heap;g_MatrixHeap=&foreign_heap;
        NativeInitRuntime refused;bool refused_owner=false;
        try{refused.Begin(nullptr,nullptr);}catch(const std::logic_error&){refused_owner=true;}
        assert(refused_owner && g_MatrixHeap==&foreign_heap && !g_Cache && !g_CacheHeap);
        refused.Deinit();assert(g_MatrixHeap==&foreign_heap);
        g_MatrixHeap=nullptr;
    }
    assert(!g_Config.m_Cursors && g_Config.m_CursorsCnt==0);
    NativeInitRuntime absent;bool rejected=false;
    try{absent.InitializeGameDefaults();}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected && !absent.GameDefaultsReady());
}
namespace {
struct RefusingCursorWindow final:platform3g::Window {
    platform3g::Window& real;
    explicit RefusingCursorWindow(platform3g::Window& w):real(w){}
    void* Identity()const noexcept override{return real.Identity();}
    std::unique_ptr<platform3g::WindowAttachment> Attach(platform3g::MessageHandler h)override{return real.Attach(h);}
    void Show(platform3g::Visibility v)override{real.Show(v);}
    void ReleaseCursor()override{real.ReleaseCursor();}
    std::intptr_t DefaultMessage(std::uint32_t m,std::uintptr_t w,std::intptr_t l)override{return real.DefaultMessage(m,w,l);}
    platform3g::CursorHandle ReadCursor()override{return real.ReadCursor();}
    platform3g::CursorHandle ExchangeCursor(platform3g::CursorHandle)override{throw platform3g::Unsupported("cursor denied by backend");}
};
}
void VerifyBootstrapCursor(NativeInitRuntime& runtime,platform3g::Window& window,platform3g::Device& device,bool negotiated) {
    assert(g_MatrixHeap && g_MatrixHeap!=g_CacheHeap && !runtime.Cursor());
    if(!negotiated) {
        bool rejected=false;try{runtime.InitializeCursor();}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected && !runtime.Cursor() && runtime.GameDefaultsReady());
        rejected=false;try{CMatrixCursor missing_window;}catch(const platform3g::Unsupported&){rejected=true;}
        assert(rejected); // Actual constructor unwinds a valid wstring, not memset memory.
        std::puts("original cursor constructor/window prefix Unsupported; CPU game heap preserved");return;
    }
    window.Show(platform3g::Visibility::Normal);window.Update();
    const auto borrowed=window.ReadCursor();assert(borrowed.value!=0);
    runtime.DetachDevice();RefusingCursorWindow denied(window);
    runtime.AttachDevice(denied,device);
    bool rejected=false;try{runtime.InitializeCursor();}catch(const platform3g::Unsupported&){rejected=true;}
    assert(rejected && !runtime.Cursor() && runtime.GpuReady() && runtime.GameDefaultsReady());
    assert(window.ReadCursor()==borrowed);
    runtime.DetachDevice();runtime.AttachDevice(window,device);runtime.InitializeCursor();
    assert(runtime.Cursor() && window.ReadCursor().value==0);
    assert(runtime.Cursor()->GetPosX()==0 && runtime.Cursor()->GetPosY()==0);
    runtime.Cursor()->SetPos(37,-11);
    assert(runtime.Cursor()->GetPosX()==37 && runtime.Cursor()->GetPosY()==-11);
    bool duplicate=false;try{runtime.InitializeCursor();}catch(const std::logic_error&){duplicate=true;}
    assert(duplicate && runtime.Cursor() && window.ReadCursor().value==0);
    runtime.Cursor()->Clear();runtime.Cursor()->Clear();assert(window.ReadCursor()==borrowed);
    runtime.DetachDevice();assert(!runtime.Cursor() && window.ReadCursor()==borrowed && g_MatrixHeap);
    runtime.AttachDevice(window,device);runtime.InitializeCursor();assert(window.ReadCursor().value==0);
    std::puts("original cursor ctor/Clear EXECUTED through actual SetCursor/GetCursor; refusal, repeated Clear and detach/reentry restore PASS; icon/texture selection NOT RUN");
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
