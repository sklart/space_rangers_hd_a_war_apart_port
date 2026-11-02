// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_diagnostic_smoke.hpp"
#include "native_diagnostic_pixels.hpp"
#include "platform_switch.hpp"
#include "native_debug_font.hpp"
#include "native_texture.hpp"
#include "native_gamma.hpp"
#include <switch.h>
#include <cstring>
namespace srhd_awa::matrixgame::diagnostic {
namespace p=platform3g;
void RenderStatus(p::DekoDevice& device,std::span<const wchar_t* const> lines){
    Operation operation("diagnostic.status_screen");
    auto state=device.CaptureState();if(!state)throw std::runtime_error("diagnostic status has no state owner");
    struct Guard{p::DekoDevice& d;std::unique_ptr<p::DeviceStateLease>& s;bool done{};
        ~Guard(){if(!done){const auto aborted=d.AbortNativeFrame();Record(aborted<0?"failed":"result","diagnostic.status.abort",{},aborted);
            if(s){const auto restored=s->Restore();Record(restored<0?"failed":"result","diagnostic.status.restore",{},restored);}}}} guard{device,state};
    const auto extent=device.DrawableExtent();const int width=extent.right-extent.left;
    {Operation acquire("diagnostic.status.acquire_clear");device.BeginNativeFrame(0xff152638);p::CheckDeviceResult(device.NativeBeginScene(),"status BeginScene");}
    auto font=debug_font::native::Font::Create(device.DebugFontSession());unsigned y=24;
    for(const auto* line:lines){debug_font::TextRect box{24,int(y),width-24,int(y+28)};font->Draw(line,-1,box,0x120,0xffffffff);y+=30;}
    p::CheckDeviceResult(device.NativeEndScene(),"status EndScene");device.PresentNativeFrame();font.reset();
    p::CheckDeviceResult(state->Restore(),"status restore original device state");state.reset();guard.done=true;
}
void SaveFrame(Journal& journal,std::string_view name,unsigned width,unsigned height,std::span<const std::byte> bgra){
    Operation operation("GPU.primary.readback.write");ValidateFrame(width,height,bgra);
    if(name!="smoke" && name!="gamma-smoke" && name!="orientation-smoke" && name!="original-first")throw std::invalid_argument("diagnostic frame filename");
    std::array<unsigned char,32> digest{};sha256CalculateHash(digest.data(),bgra.data(),bgra.size());std::array<char,65> hex{};
    for(unsigned i=0;i<32;++i)std::snprintf(hex.data()+i*2,3,"%02x",digest[i]);
    Record("framebuffer_sha256",name,hex.data());Metric("framebuffer_width",width);Metric("framebuffer_height",height);
    const auto path=journal.Directory()/(std::string(name)+".bgra");FILE* file=std::fopen(path.c_str(),"wb");
    if(!file)throw std::runtime_error("diagnostic framebuffer open failed");
    struct Owner{FILE* f;~Owner(){if(f)std::fclose(f);}} owner{file};
    const auto io=SystemIO();if(io.write(bgra.data(),bgra.size(),file)!=bgra.size() || io.flush(file))throw std::runtime_error("diagnostic framebuffer write/flush failed");
    owner.f=nullptr;if(io.close(file))throw std::runtime_error("diagnostic framebuffer close failed");
}
void RunSmoke(p::DekoDevice& device,Journal& journal){
    Operation operation("A.real_GPU_smoke");
    auto state=device.CaptureState();if(!state)throw std::runtime_error("smoke CaptureState returned no owner");
    struct Restore {p::DekoDevice& d;std::unique_ptr<p::DeviceStateLease>& s;bool done{};
        ~Restore(){if(!done){const auto aborted=d.AbortNativeFrame();Record("result","A.smoke.abort",{},aborted);if(s){const auto restored=s->Restore();Record(restored<0?"failed":"result","A.smoke.restore",{},restored);}}}} restore{device,state};
    IDirect3DTexture9* texture{};
    struct TextureOwner {IDirect3DTexture9*& p;~TextureOwner(){if(p)native_texture::Release(p);}} texture_owner{texture};
    {
        Operation upload("A.BC1.upload");
        p::CheckDeviceResult(native_texture::Create(device.TextureSession(),4,4,1,0,0x31545844,1,&texture),"smoke native BC1 Create");
        legacy_texture::LockedRect lock;
        p::CheckDeviceResult(native_texture::Lock(texture,0,&lock,nullptr,0),"smoke BC1 Lock");
        if(lock.Pitch!=8 || !lock.pBits)throw std::runtime_error("smoke BC1 tight block pitch");
        std::memcpy(lock.pBits,SmokeBC1.data(),SmokeBC1.size());p::CheckDeviceResult(native_texture::Unlock(texture,0),"smoke BC1 Unlock");
        native_texture::Preload(texture);Metric("smoke_BC1_bytes",SmokeBC1.size());
    }
    const auto extent=device.DrawableExtent();const unsigned width=unsigned(extent.right-extent.left),height=unsigned(extent.bottom-extent.top);
    const auto vertices=SmokeQuad(width,height);
    p::CheckDeviceResult(device.SetFixedVertexLayout(0x144),"smoke XYZRHW diffuse tex1");
    for(const auto [reg,value]:std::array<std::pair<unsigned,unsigned>,7>{{{7,0},{14,0},{22,1},{27,0},{28,0},{137,0},{141,0}}})
        p::CheckDeviceResult(device.SetRenderState(static_cast<RasterRegister>(reg),value),"smoke raster state");
    for(const auto [reg,value]:std::array<std::pair<unsigned,unsigned>,6>{{{1,2},{2,2},{4,2},{5,2},{11,0},{24,0}}})
        p::CheckDeviceResult(device.SetTextureStageState(0,static_cast<p::TextureStageRegister>(reg),value),"smoke stage0 texture select");
    p::CheckDeviceResult(device.SetTextureStageState(1,static_cast<p::TextureStageRegister>(1),1),"smoke disable stage1");
    p::CheckDeviceResult(device.BindBorrowedTexture(0,texture),"smoke bind BC1");
    {Operation acquire("A.frame.acquire_clear");device.BeginNativeFrame(0xff152638);p::CheckDeviceResult(device.NativeBeginScene(),"smoke BeginScene");}
    {Operation draw("A.BC1.draw");p::CheckDeviceResult(device.NativeDrawUP(5,2,std::as_bytes(std::span{vertices}),sizeof(SmokeVertex)),"smoke diagnostic rectangle");}
    // Original MS Sans Serif debug-font asset and real deko upload/draw.
    std::shared_ptr<debug_font::native::Font> font;
    {Operation draw("A.debug_font.upload_draw");font=debug_font::native::Font::Create(device.DebugFontSession());
    debug_font::TextRect text{24,24,int(width)-24,56};font->Draw(L"MatrixGame diagnostic GPU smoke",-1,text,0x120,0xffffffff);}
    p::CheckDeviceResult(device.NativeEndScene(),"smoke EndScene");
    std::vector<std::byte> source_pixels;
    {
        Operation readback("A.BC1.framebuffer_check");
        const auto surfaces=device.SurfaceSession();auto pixels=native_surfaces::ReadFor(surfaces,surfaces->primary_color);
        CheckSmokePixels(width,height,pixels);SaveFrame(journal,"smoke",width,height,pixels);
        source_pixels=std::move(pixels);
    }
    // Preserve an asymmetric frame through the actual presentation shader.
    // A constant gamma LUT cannot expose a vertical/horizontal inversion.
    device.ApplyCalibratedGamma(0,p::IdentityGamma());
    device.RequestPresentationReadback([&journal,source=std::move(source_pixels)](auto pixels,unsigned w,unsigned h){
        Operation check("A.orientation.presentation_check");CheckIdentityPresentation(w,h,source,pixels);SaveFrame(journal,"orientation-smoke",w,h,pixels);
    });
    device.PresentNativeFrame();
    device.BeginNativeFrame(0xff152638);p::CheckDeviceResult(device.NativeBeginScene(),"gamma smoke BeginScene");
    p::CheckDeviceResult(device.NativeDrawUP(5,2,std::as_bytes(std::span{vertices}),sizeof(SmokeVertex)),"gamma smoke diagnostic rectangle");
    {debug_font::TextRect text{24,24,int(width)-24,56};font->Draw(L"MatrixGame diagnostic GPU smoke",-1,text,0x120,0xffffffff);}
    p::CheckDeviceResult(device.NativeEndScene(),"gamma smoke EndScene");
    device.ApplyCalibratedGamma(0,SmokeGammaRamp());
    device.RequestPresentationReadback([&journal](auto pixels,unsigned width,unsigned height){
        Operation check("A.gamma.presentation_check");CheckGammaSmokePixels(width,height,pixels);SaveFrame(journal,"gamma-smoke",width,height,pixels);
    });
    device.PresentNativeFrame();font.reset();
    p::CheckDeviceResult(state->Restore(),"smoke restore original device state");state.reset();restore.done=true;
    native_texture::Release(texture);texture=nullptr;journal.RequireHealthy();
}
}
