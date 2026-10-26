// Exact original D3DX font bake and independent native-layout HAL oracle.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include "native_debug_font.hpp"
#include <array>
#include <fstream>
#include <map>
#include <set>
#include <cstdio>
#include <cassert>
#include <cstring>
namespace f=srhd_awa::matrixgame::debug_font::native;
void Check(HRESULT value){if(FAILED(value))throw std::runtime_error("real D3D9/GDI font operation failed: "+std::to_string(unsigned(value)));}
template<class T>struct Com {T* p{};~Com(){if(p)p->Release();}T* operator->()const{return p;}};
struct Device {
    HWND window{};Com<IDirect3D9> api;Com<IDirect3DDevice9> device;Com<ID3DXFont> font;
    Device(){window=CreateWindowW(L"STATIC",L"MatrixGame original font oracle",WS_OVERLAPPEDWINDOW,0,0,1024,768,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
        api.p=Direct3DCreate9(D3D_SDK_VERSION);D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
        Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device.p));
        Check(D3DXCreateFontW(device.p,25,0,FW_NORMAL,1,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH,L"MS Sans Serif",&font.p));}
    ~Device(){if(window)DestroyWindow(window);}
};
struct Target {
    IDirect3DDevice9* device;Com<IDirect3DSurface9> surface,readback;
    unsigned w,h;
    Target(IDirect3DDevice9* d,unsigned width,unsigned height):device(d),w(width),h(height){
        Check(d->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&surface.p,nullptr));
        Check(d->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback.p,nullptr));}
    void Begin(){Check(device->SetRenderTarget(0,surface.p));Check(device->SetDepthStencilSurface(nullptr));
        D3DVIEWPORT9 viewport{0,0,w,h,0,1};Check(device->SetViewport(&viewport));Check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0));Check(device->BeginScene());}
    std::vector<std::byte> End(){Check(device->EndScene());Check(device->GetRenderTargetData(surface.p,readback.p));D3DLOCKED_RECT lock{};Check(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        std::vector<std::byte> bytes(std::size_t(w)*h*4);for(unsigned y=0;y<h;++y)std::memcpy(bytes.data()+std::size_t(y)*w*4,static_cast<std::byte*>(lock.pBits)+std::size_t(y)*lock.Pitch,w*4);Check(readback->UnlockRect());return bytes;}
};
void Write(const char* path,std::span<const std::byte> bytes){std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!stream)throw std::runtime_error("font artifact write");}
struct Atlas final:f::Atlas {Com<IDirect3DTexture9> texture;};
int main(int argc,char** argv){try{
    assert(argc==2);Device owner;auto* device=owner.device.p;auto* font=owner.font.p;const auto dc=font->GetDC();TEXTMETRICW metrics{};assert(font->GetTextMetricsW(&metrics));
    wchar_t face[128]{};GetTextFaceW(dc,128,face);assert(std::wstring(face)==L"MS Sans Serif" && metrics.tmHeight==25);
    std::wprintf(L"original D3DX face=%ls h=%d ascent=%d descent=%d average=%d maximum=%d charset=%d\n",face,metrics.tmHeight,metrics.tmAscent,metrics.tmDescent,metrics.tmAveCharWidth,metrics.tmMaxCharWidth,metrics.tmCharSet);
    std::vector<wchar_t> characters(65536);std::vector<WORD> indices(65536);for(unsigned cp=0;cp<65536;++cp)characters[cp]=wchar_t(cp);
    for(unsigned cp=0;cp<65536;cp+=4096)assert(GetGlyphIndicesW(dc,characters.data()+cp,4096,indices.data()+cp,GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR);
    const auto range_bytes=GetFontUnicodeRanges(dc,nullptr);assert(range_bytes);std::vector<std::byte> range_storage(range_bytes);auto* ranges=reinterpret_cast<GLYPHSET*>(range_storage.data());assert(GetFontUnicodeRanges(dc,ranges)==range_bytes);
    std::map<unsigned,unsigned> representatives;std::vector<unsigned> cps;
    for(unsigned range=0;range<ranges->cRanges;++range)for(unsigned n=0;n<ranges->ranges[range].cGlyphs;++n){const unsigned cp=ranges->ranges[range].wcLow+n;
        if(cp<32 || (cp>=0x7f && cp<0xa0) || indices[cp]==0xffff)continue;
        if(!representatives.contains(indices[cp])){representatives[indices[cp]]=unsigned(cps.size());cps.push_back(cp);}}
    const unsigned width=512,height=((unsigned(cps.size())+15)/16)*32;assert(metrics.tmMaxCharWidth<28 && metrics.tmHeight<29);
    Target target(device,width,height);target.Begin();std::vector<f::Glyph> glyphs;
    for(unsigned n=0;n<cps.size();++n){const wchar_t ch=wchar_t(cps[n]);SIZE extent{};assert(GetTextExtentPoint32W(dc,&ch,1,&extent));
        const unsigned x=(n%16)*32,y=(n/16)*32;RECT rect{LONG(x+2),LONG(y+2),LONG(x+32),LONG(y+32)};
        const wchar_t literal[2]{ch,0};const int count=1;
        const int drawn=font->DrawTextW(nullptr,literal,count,&rect,0x120,0xffffffff);assert(drawn>0);
        glyphs.push_back({cps[n],unsigned(extent.cx),x,y,32,32});}
    const auto pixels=target.End();std::vector<std::byte> binary;auto word=[&](unsigned value){for(unsigned i=0;i<4;++i)binary.push_back(std::byte(value>>(i*8)));};
    for(char ch:std::string("MGDF25A1"))binary.push_back(std::byte(ch));
    for(unsigned value:{width,height,unsigned(metrics.tmHeight),unsigned(metrics.tmAscent),unsigned(metrics.tmDescent),unsigned(metrics.tmAveCharWidth),unsigned(metrics.tmMaxCharWidth),unsigned(metrics.tmCharSet),unsigned(glyphs.size())})word(value);
    for(const auto& g:glyphs)for(unsigned value:{g.codepoint,g.advance,g.x,g.y,g.width,g.height})word(value);
    for(unsigned cp=0;cp<65536;++cp){const auto found=representatives.find(indices[cp]);const unsigned index=found==representatives.end()?0xffff:found->second;binary.push_back(std::byte(index));binary.push_back(std::byte(index>>8));}
    unsigned ink=0;for(std::size_t p=0;p<pixels.size();p+=4){assert(pixels[p]==pixels[p+1] && pixels[p]==pixels[p+2]);const auto alpha=pixels[p];ink+=alpha!=std::byte{};
        binary.insert(binary.end(),{std::byte{255},std::byte{255},std::byte{255},alpha});}
    assert(ink>0);Write(argv[1],binary);auto data=f::Data::Load(binary);
    for(wchar_t cp:{wchar_t(0x0378),wchar_t(0x4e2d)}){wchar_t glyph{};int advance{};GCP_RESULTSW placement{};placement.lStructSize=sizeof(placement);placement.nGlyphs=1;placement.lpGlyphs=&glyph;placement.lpDx=&advance;GetCharacterPlacementW(dc,&cp,1,1024,&placement,GCP_USEKERNING);
        std::printf("outside_face cp=%x GI=%u GCP=%u advance=%d native_map=%u FONT_LINKING_NOT_PORTED\n",unsigned(cp),indices[cp],unsigned(glyph),advance,data->map[cp]);}
    auto session=std::make_shared<f::Session>();session->data=data;
    session->upload=[&](const f::Data& source){auto atlas=std::make_shared<Atlas>();Check(device->CreateTexture(source.width,source.height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&atlas->texture.p,nullptr));
        D3DLOCKED_RECT lock{};Check(atlas->texture->LockRect(0,&lock,nullptr,0));for(unsigned y=0;y<source.height;++y)std::memcpy(static_cast<std::byte*>(lock.pBits)+std::size_t(y)*lock.Pitch,source.rgba.data()+std::size_t(y)*source.width*4,source.width*4);Check(atlas->texture->UnlockRect(0));return atlas;};
    session->draw=[&](const std::shared_ptr<f::Atlas>& opaque,std::span<const f::Quad> quads){auto atlas=std::dynamic_pointer_cast<Atlas>(opaque);assert(atlas);
        Com<IDirect3DStateBlock9> state;Check(device->CreateStateBlock(D3DSBT_ALL,&state.p));
        Check(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));Check(device->SetTexture(0,atlas->texture.p));
        Check(device->SetRenderState(D3DRS_ZENABLE,FALSE));Check(device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));Check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
        Check(device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));Check(device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));Check(device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));Check(device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
        for(auto reg:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})Check(device->SetTextureStageState(0,reg,D3DTOP_MODULATE));
        for(auto reg:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})Check(device->SetTextureStageState(0,reg,D3DTA_TEXTURE));
        for(auto reg:{D3DTSS_COLORARG2,D3DTSS_ALPHAARG2})Check(device->SetTextureStageState(0,reg,D3DTA_DIFFUSE));Check(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE));
        for(auto reg:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})Check(device->SetSamplerState(0,reg,D3DTEXF_POINT));Check(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE));
        struct V{float x,y,z,rhw;DWORD color;float u,v;};std::vector<V> vertices;for(const auto& q:quads){const V a{q.left-0.5f,q.top-0.5f,0,1,q.color,q.u0,q.v0},b{q.right-0.5f,q.top-0.5f,0,1,q.color,q.u1,q.v0},c{q.left-0.5f,q.bottom-0.5f,0,1,q.color,q.u0,q.v1},d{q.right-0.5f,q.bottom-0.5f,0,1,q.color,q.u1,q.v1};vertices.insert(vertices.end(),{a,b,c,c,b,d});}
        if(!vertices.empty())Check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),vertices.data(),sizeof(V)));Check(state->Apply());};
    auto native=f::Font::Create(session);unsigned cases=0;
    std::vector<std::wstring> texts{L"MatrixGame",L"AV To ijg",L"\u041c\u0430\u0442\u0440\u0438\u0446\u0430 123",L"",L" ",L"A A ",L"A&&B",L"A&B",L"123.45 -67",L"a\tb",L"a\r\nb"};
    for(const auto& glyph:glyphs)texts.push_back(std::wstring(1,wchar_t(glyph.codepoint)));
    std::vector<std::byte> golden;auto golden_word=[&](unsigned value){for(unsigned i=0;i<4;++i)golden.push_back(std::byte(value>>(8*i)));};
    for(const auto& text:texts)for(unsigned flags:{0x124u,0x120u,0x420u}){
        RECT original{10,10,250,29};srhd_awa::matrixgame::debug_font::TextRect portable{10,10,250,29};Target line(device,512,64);
        line.Begin();const int expected=font->DrawTextW(nullptr,text.c_str(),int(text.size()),&original,flags,0xffffffff);auto reference=line.End();
        line.Begin();const int result=native->Draw(text.c_str(),int(text.size()),portable,flags,0xffffffff);auto actual=line.End();
        unsigned different=0,max_diff=0;for(std::size_t p=0;p<actual.size();p+=4)for(unsigned c=0;c<3;++c){const auto diff=unsigned(std::abs(int(actual[p+c])-int(reference[p+c])));different+=diff!=0;max_diff=std::max(max_diff,diff);}
        std::printf("case=%u count=%zu flags=%x result=%d/%d rect=%ld,%ld/%d,%d pixels=%u max=%u\n",cases,text.size(),flags,expected,result,original.right,original.bottom,portable.right,portable.bottom,different,max_diff);
        if(expected!=result || original.right!=portable.right || original.bottom!=portable.bottom || different){Write((std::string(argv[1])+".expected.bgra").c_str(),reference);Write((std::string(argv[1])+".actual.bgra").c_str(),actual);throw std::runtime_error("native font oracle mismatch");}
        golden_word(unsigned(text.size()));for(wchar_t cp:text)golden_word(unsigned(cp));golden_word(flags);golden_word(unsigned(expected));
        for(int value:{int(original.left),int(original.top),int(original.right),int(original.bottom)})golden_word(unsigned(value));
        for(std::size_t p=0;p<reference.size();p+=4)golden.push_back(reference[p]);
        ++cases;
    }
    unsigned alpha_cases=0;
    for(DWORD color:{0x00ffffffu,0x40ffffffu,0x80ffffffu,0xc0ffffffu}){
        Target line(device,512,64);RECT original{10,10,250,29};srhd_awa::matrixgame::debug_font::TextRect rect{10,10,250,29};
        line.Begin();font->DrawTextW(nullptr,L"Fade \u0416",-1,&original,0x124,color);const auto expected=line.End();
        line.Begin();native->Draw(L"Fade \u0416",-1,rect,0x124,color);const auto actual=line.End();
        for(std::size_t p=0;p<actual.size();p+=4)for(unsigned c=0;c<3;++c)assert(actual[p+c]==expected[p+c]);++alpha_cases;
    }
    Check(font->OnLostDevice());native->Lost();Check(font->OnResetDevice());native->Reset();
    {Target line(device,512,64);RECT original{10,10,250,29};srhd_awa::matrixgame::debug_font::TextRect rect{10,10,250,29};
        line.Begin();font->DrawTextW(nullptr,L"Reset",-1,&original,0x124,~0u);const auto expected=line.End();
        line.Begin();native->Draw(L"Reset",-1,rect,0x124,~0u);const auto actual=line.End();
        for(std::size_t p=0;p<actual.size();p+=4)for(unsigned c=0;c<3;++c)assert(actual[p+c]==expected[p+c]);}
    session->Close();native.reset();
    Write((std::string(argv[1])+".goldens").c_str(),golden);
    std::printf("original font bake/HAL PASS glyphs=%zu atlas=%ux%u ink=%u cases=%u alpha_cases=%u after_reset_pixels=matched SWITCH_HARDWARE_NOT_RUN\n",glyphs.size(),width,height,ink,cases,alpha_cases);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"font oracle stopped: %s\n",e.what());return 10;}}
