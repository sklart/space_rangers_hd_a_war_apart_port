// Actual original Text::Render path for the first Top/mm static label.
// Missing-provider boundary only; successful original text execution has its own test.
#include "platform_3g.hpp"
#include "Text/Render.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
namespace p=srhd_awa::matrixgame::platform3g;
struct Routing final:p::Device{
    p::DeviceCaps ReadCaps()override{throw p::Unsupported("negative text boundary has no GPU caps");}
    std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("negative text boundary has no GPU budget");}
    p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("negative text boundary has no display gamma");}
    void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
int main(){Routing routing;assert(!p::Current().ready);p::Current().ready=true;p::Current().device=&routing;
    CBitmap bitmap;bool stopped=false;
    try{Text::Render(L"Меню",L"Font.2Ranger",0xffb0ab99,75,20,1,1,0,0,-2,Base::CRect(0,0,75,20),bitmap);}
    catch(const p::Unsupported& error){stopped=std::strcmp(error.what(),"native UI text requires native texture provider")==0;}
    assert(stopped && !bitmap.Data());p::Current().device=nullptr;p::Current().ready=false;
    std::puts("original Text::Render Top/mm Font.2Ranger negative guard: native UI text requires native texture provider PASS NO_NATIVE_INIT NO_GPU SWITCH_HARDWARE_NOT_RUN");
}
