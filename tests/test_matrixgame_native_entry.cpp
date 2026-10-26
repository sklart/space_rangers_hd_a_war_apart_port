#ifdef NDEBUG
#error Native entry test requires assertions.
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "skin_callbacks.hpp"
#include <cassert>
#include <cstdio>
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
    }
    assert(device.capabilities==2);
    std::puts("native entry executed original Init to material caps with explicit output binding; no D3D identities/Win32 hooks; two original cleanup cycles/output unbind PASS; native GPU NOT RUN");
}
