#ifdef NDEBUG
#error Native entry test requires assertions.
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "skin_callbacks.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace srhd_awa::matrixgame;
namespace p=platform3g;
struct Window final:p::Window {
    int attachments{},win32_calls{};
    void* Identity()const noexcept override{return const_cast<Window*>(this);}
    p::WindowRect ReadClientRect()override{return {0,0,1280,720};} // Fixture extent, no display claim.
    struct Lease final:p::WindowAttachment {
        Window& owner;bool live{true};explicit Lease(Window& w):owner(w){++owner.attachments;}
        void Restore()noexcept override{if(live){--owner.attachments;live=false;}}
        ~Lease(){Restore();}
    };
    std::unique_ptr<p::WindowAttachment> AttachNative()override{return std::make_unique<Lease>(*this);}
    std::unique_ptr<p::WindowAttachment> Attach(p::MessageHandler)override{++win32_calls;throw std::logic_error("Win32 attachment called in native entry");}
    void Show(p::Visibility)override{++win32_calls;throw std::logic_error("fixture cannot show Win32 window");}
    void ReleaseCursor()override{++win32_calls;throw std::logic_error("fixture has no Win32 cursor");}
    std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)override{++win32_calls;throw std::logic_error("fixture has no Win32 messages");}
};
struct Device final:p::Device {
    int capabilities{},identities{};
    p::LegacyDeviceHandles ReadLegacyHandles()override{++identities;throw std::logic_error("native entry asked for D3D identities");}
    p::DeviceCaps ReadCaps()override{
        ++capabilities;assert(g_MatrixHeap && g_MatrixData);
        assert(output_paths::NativeBound()); // Original Init has run, not just its owner bootstrap.
        throw p::Unsupported("NATIVE_MATERIAL_CAPS_PENDING");
    }
    p::GammaRamp ReadGamma(std::uint32_t)override{throw std::logic_error("fixture cannot supply gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
    std::uint64_t AvailableTextureBytes()override{throw std::logic_error("fixture cannot report GPU memory");}
};
int main(){
    Window window;Device device;
    for(int i=0;i<2;++i){
        NativeInitRuntime runtime;NativeGameOptions options{window,device};options.seed=37;
        auto requested=SRobotsSettings::generate_default_settings();
        requested.m_SkyBox=0;requested.m_IzvratMS=true;requested.m_AFDegree=24;
        requested.m_Brightness=.625f;requested.m_Contrast=.375f;
        if(i)options.settings=&requested; // Same original Init prefix with copied caller settings.
        // Binding only: fixture stops before any filesystem creation.
#ifdef _WIN32
        options.output_root=L"C:/matrixgame-native-entry-test-output";
#else
        options.output_root=L"/matrixgame-native-entry-test-output";
#endif
        bool refused=false;
        try{runtime.InitializeNativeGame(options);}catch(const p::Unsupported& error){refused=std::string(error.what())=="NATIVE_MATERIAL_CAPS_PENDING";}
        assert(refused && !runtime.CpuReady() && !runtime.OriginalGameReady());
        assert(!output_paths::NativeBound());
        assert(!g_MatrixHeap && !g_MatrixMap && !g_MatrixData && !g_Cache && !g_CacheHeap);
        assert(!window.attachments && !window.win32_calls && !device.identities && !BorrowedSkinLeaseCount());
        assert(requested.m_ResolutionX==0 && requested.m_ResolutionY==0 && requested.m_AFDegree==24 && requested.m_Brightness==.625f);
    }
    assert(device.capabilities==2);
    unsigned early_rejections{};
    for(unsigned which=0;which<7;++which){
        NativeInitRuntime runtime;NativeGameOptions options{window,device};auto requested=SRobotsSettings::generate_default_settings();
        options.settings=&requested;options.output_root=L""; // Would fail its own binding if preflight happened too late.
        if(which==0)requested.FDirect3D=reinterpret_cast<void*>(std::uintptr_t(1));
        if(which==1)requested.FD3DDevice=reinterpret_cast<void*>(std::uintptr_t(1));
        if(which==2){requested.m_ResolutionX=800;requested.m_ResolutionY=600;}
        if(which==3)requested.m_ResolutionX=1280;
        if(which==4)requested.m_Brightness=std::numeric_limits<float>::quiet_NaN();
        if(which==5)requested.m_Contrast=std::numeric_limits<float>::infinity();
        if(which==6)requested.m_MaxDistance=-std::numeric_limits<float>::infinity();
        bool rejected=false;try{runtime.InitializeNativeGame(options);}catch(const std::invalid_argument& error){
            const std::string what=error.what();rejected=what.starts_with("native settings") || what=="nonfinite native game settings";}
        assert(rejected && !runtime.CpuReady() && !output_paths::NativeBound() && !device.identities && device.capabilities==2);
        assert(!g_MatrixHeap && !g_MatrixData && !g_MatrixMap && !g_Cache && !g_CacheHeap && !window.attachments);++early_rejections;
    }
    std::printf("native settings provided_entry=1 rejected_before_Init=%u caller_unchanged=PASS\n",early_rejections);
    std::puts("native entry executed original Init to material caps with explicit output binding; no D3D identities/Win32 hooks; two original cleanup cycles/output unbind PASS; native GPU NOT RUN");
}
