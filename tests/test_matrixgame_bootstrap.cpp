// Integrated existing cache/VO/material owner -> original DLL bootstrap.
// Real Windows SDK device when available; explicit unsupported elsewhere.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "skin_pipeline.hpp"
#include MATRIXGAME_FULL_VECTOR_HEADER // Full declaration must agree with cache/CPU class.
#include MATRIXGAME_FULL_BILLBOARD_HEADER
#include MATRIXGAME_FULL_3G_HEADER
#include "VectorObjectAnimNative.hpp"
#include "Form.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <type_traits>
using namespace srhd_awa::matrixgame;
namespace p=srhd_awa::matrixgame::platform3g;
// Original full animator retains its light pointer/count. The accepted native
// no-light variant remains a separate class, never substituted for this type.
constexpr auto full_light_extent=(sizeof(void*)+sizeof(int)+alignof(CVectorObjectAnim)-1)/alignof(CVectorObjectAnim)*alignof(CVectorObjectAnim);
static_assert(sizeof(CVectorObjectAnim)==sizeof(NativeVectorObjectAnim)+full_light_extent);
static_assert(sizeof(SBillboardVertex)==24); // Original XYZ/diffuse/UV payload, not MeshVertex.
static_assert(std::is_base_of_v<CCacheData,CBaseTexture> && std::is_base_of_v<CBaseTexture,CTextureManaged>);
static_assert(std::is_abstract_v<CBaseTexture> && !std::is_abstract_v<CTextureManaged>);
static_assert(std::is_same_v<decltype(std::declval<CTextureManaged&>().DX()),IDirect3DTexture9*>);
static_assert(TF_NOMIPMAP==8 && TF_ALPHATEST==16 && TF_ALPHABLEND==32);
// SDK enum -> bool would silently select convert_to_16bit. The declaration
// boundary must require explicit format conversion, not change texture behavior.
enum LegacyTextureFormatForOverloadCheck {LegacyFormatValueForCheck=21};
template<class T> concept TextureBitmapArgument=requires(CTextureManaged& t,const CBitmap& bm,T arg){t.LoadFromBitmap(bm,arg);};
static_assert(TextureBitmapArgument<bool> && TextureBitmapArgument<TexturePixelFormat>);
static_assert(!TextureBitmapArgument<LegacyTextureFormatForOverloadCheck>);
extern void VerifyBootstrapSampling(NativeInitRuntime&,p::Window&,p::Device&,unsigned,bool);
extern void VerifyBootstrapSamplerCold();
extern void VerifyBootstrapDefaults(NativeInitRuntime&,p::Window&,p::Device&,unsigned,bool);
extern void VerifyBootstrapGameCold();
extern void VerifyBootstrapGameDefaults(NativeInitRuntime&);
extern void VerifyBootstrapProjection();
extern void VerifyBootstrapCursor(NativeInitRuntime&,p::Window&,p::Device&,bool);
struct UnavailableDevice final:p::Device {
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("backend has no verified legacy device caps");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("gamma unsupported");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("texture memory query unsupported");}
};
struct UnavailableWindow final:p::Window {
    void* Identity()const noexcept override{return nullptr;}
    std::unique_ptr<p::WindowAttachment> Attach(p::MessageHandler)override{throw p::Unsupported("this platform has no native window attachment");}
    void Show(p::Visibility)override{throw p::Unsupported("window visibility unsupported");}
    void ReleaseCursor()override{throw p::Unsupported("cursor unsupported");}
    std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)override{throw p::Unsupported("window messages unsupported");}
};
struct Form final:CForm {
    unsigned keys{},mouse{},events{};
    void Enter()override{} void Leave()override{} void Draw()override{} void Takt(int)override{}
    void MouseMove(int x,int y)override{assert(x==-3 && y==7);++mouse;}
    void MouseKey(ButtonStatus,int,int,int)override{}
    void Keyboard(bool down,std::uint8_t key)override{assert(down && key==65);++keys;}
    void SystemEvent(ESysEvent)override{++events;}
};
void cold(){assert(!g_Cache && !g_CacheHeap && !InspectCpu3GState().bound && !p::Current().ready && !p::Current().attachment);assert(!CachedTextureBytes());VerifyBootstrapSamplerCold();VerifyBootstrapGameCold();}
int RunBootstrap(int argc,char**argv,void* module,p::Window& bound_window,p::Device& bound_device) {
    if(argc!=3)return 2;
    std::setvbuf(stdout,nullptr,_IONBF,0);
    try {
        VerifyBootstrapProjection();
        D3DXMATRIX billboard_rotation;
        assert(D3DXMatrixRotationZ(&billboard_rotation,1.57079632679f)==&billboard_rotation);
        assert(std::fabs(billboard_rotation._11)<1e-6f && std::fabs(billboard_rotation._12-1.f)<1e-6f);
        assert(std::fabs(billboard_rotation._21+1.f)<1e-6f && billboard_rotation._33==1.f && billboard_rotation._44==1.f);
        const auto model=std::filesystem::path(argv[1]).wstring();
        const auto texture=std::filesystem::path(argv[2]).wstring();
        UnavailableDevice unavailable;
        auto* window=&bound_window;auto* device=&bound_device;
#ifdef _WIN32
        const auto hwnd=static_cast<HWND>(window->Identity());
        const auto original_proc=GetWindowLongPtrW(hwnd,GWLP_WNDPROC);
#endif
        cold();
        for(unsigned cycle=0;cycle<2;++cycle) {
            std::printf("cycle=%u entering original cache/CPU bootstrap\n",cycle);
            NativeInitRuntime runtime;runtime.Begin(module,window->Identity());
            std::puts("CacheInit + L3G CPU prefix EXECUTED");
            MaterialTexture stale;
            {
                NativeSkinLibrary skins(runtime,false);NativeSkinPipeline pipeline(runtime,skins);
                auto* object=static_cast<CVectorObject*>(g_Cache->Get(CacheClass::VO,model));
                std::puts("Cache Get(VO) EXECUTED; entering original LoadSpecial");
                pipeline.Load(*object,OLF_NO_TEX,GSP_ORDINAL);
                const auto skin=pipeline.AcquireSkin(texture.c_str(),GSP_ORDINAL);
                const auto batch=pipeline.Prepare(*object,0,{},skin);
                assert(!batch.submissions.empty() && !batch.textures.empty());
                stale=BorrowMaterialTexture(*g_Cache->Get(CacheClass::TextureManaged,texture));
                (void)stale.Read();
                std::printf("cycle=%u original Cache/VO/SSkin ready submissions=%zu\n",cycle,batch.submissions.size());
#ifdef _WIN32
                runtime.AttachDevice(*window,*device);assert(runtime.GpuReady());
                VerifyBootstrapGameDefaults(runtime);
                VerifyBootstrapSampling(runtime,*window,*device,cycle,true);
                VerifyBootstrapDefaults(runtime,*window,*device,cycle,true);
                VerifyBootstrapCursor(runtime,*window,*device,true);
                assert(GetWindowLongPtrW(hwnd,GWLP_WNDPROC)!=original_proc);
                const auto actual=device->ReadCaps();
                assert(actual.swapchain_count==p::Current().caps.swapchain_count);
                std::printf("original L3GInitAsDLL EXECUTED SDK caps stages=%u anisotropy=%u swapchains=%u texMiB=%d\n",actual.max_texture_stages,actual.max_anisotropy,actual.swapchain_count,p::Current().available_texture_mib);
                // Showing a previously hidden host window can synchronously
                // deliver WM_ACTIVATEAPP itself. Stabilize visibility before
                // binding a form, then check the explicit messages exactly.
                SendMessageW(hwnd,WM_ACTIVATEAPP,TRUE,0);
                Form form;FormChange(&form);
                SendMessageW(hwnd,WM_ACTIVATEAPP,TRUE,0);
                SendMessageW(hwnd,WM_KEYDOWN,65,0);
                SendMessageW(hwnd,WM_MOUSEMOVE,0,MAKELPARAM(WORD(-3),7));
                // Minimization also generates an OS deactivation. Settle that
                // transition without a form, then test one explicit inactive
                // notification. Keep the exact two-event assertion below.
                FormChange(nullptr);ShowWindow(hwnd,SW_MINIMIZE);FormChange(&form);
                SendMessageW(hwnd,WM_ACTIVATEAPP,FALSE,0);
                std::printf("original WndProc -> CForm keys=%u mouse=%u events=%u\n",form.keys,form.mouse,form.events);
                assert(form.keys==1 && form.mouse==1 && form.events==2);
                FormChange(nullptr);
                runtime.DetachDevice();
                assert(runtime.CpuReady() && !runtime.GpuReady() && InspectCpu3GState().bound);
                assert(GetWindowLongPtrW(hwnd,GWLP_WNDPROC)==original_proc);
                runtime.AttachDevice(*window,*device);assert(runtime.GpuReady());
                runtime.InitializeCursor(); // Leave a real cursor owned until full Deinit.
#else
                VerifyBootstrapGameDefaults(runtime); // Independent original CPU settings stage; GPU remains unsupported.
                bool rejected=false;
                try{runtime.AttachDevice(*window,*device);}catch(const p::Unsupported& e){rejected=true;std::printf("GPU prefix UNSUPPORTED: %s\n",e.what());}
                assert(rejected && runtime.CpuReady() && InspectCpu3GState().bound && !runtime.GpuReady() && !p::Current().attachment);
                VerifyBootstrapSampling(runtime,*window,*device,cycle,false);
                VerifyBootstrapDefaults(runtime,*window,*device,cycle,false);
                VerifyBootstrapCursor(runtime,*window,*device,false);
#endif
            }
            runtime.Deinit();runtime.Deinit();cold();
            bool expired=false;try{(void)stale.Read();}catch(...){expired=true;}assert(expired);
#ifdef _WIN32
            assert(IsWindow(hwnd) && GetWindowLongPtrW(hwnd,GWLP_WNDPROC)==original_proc);
            assert(window->ReadCursor().value!=0);
            (void)device->ReadCaps(); // borrowed device still live
            assert(device->OutstandingStateLeases()==0);
            // New integration fault: real hook is installed, device negotiation refuses.
            runtime.Begin(module,hwnd);bool rejected=false;
            try{runtime.AttachDevice(*window,unavailable);}catch(const p::Unsupported&){rejected=true;}
            assert(rejected && !p::Current().ready && GetWindowLongPtrW(hwnd,GWLP_WNDPROC)==original_proc);
            runtime.Deinit();cold();
#endif
        }
        std::puts("BOOTSTRAP owner cleanup/reentry PASS; CGame::Init NOT CALLED; draw_executed=false");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"bootstrap failed: %s\n",e.what());return 1;}
}
#ifdef _WIN32
extern int RunWindowsBootstrap(int,char**,int(*)(int,char**,void*,p::Window&,p::Device&));
int main(int argc,char**argv){return RunWindowsBootstrap(argc,argv,RunBootstrap);}
#else
int main(int argc,char**argv){UnavailableWindow window;UnavailableDevice device;return RunBootstrap(argc,argv,nullptr,window,device);}
#endif
