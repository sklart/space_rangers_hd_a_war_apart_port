// Exact original D3DX font bake and independent native-layout HAL oracle.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include "native_debug_font.hpp"
#include "native_text.hpp"
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
        }
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

int main(int argc,char** argv){try{
 assert(argc==3);Device owner;std::ifstream input(argv[1],std::ios::binary);std::vector<char> ttf((std::istreambuf_iterator<char>(input)),{});assert(ttf.size()>1000);DWORD count{};
 HANDLE registration=AddFontMemResourceEx(ttf.data(),DWORD(ttf.size()),nullptr,&count);assert(registration && count);
 for(unsigned id=0;id<4;++id){const unsigned size=id?11+id:10;const wchar_t* name=id?L"Verdana":L"Rangers";Com<ID3DXFont> font;
 Check(D3DXCreateFontW(owner.device.p,size,0,FW_NORMAL,1,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,name,&font.p));
 auto dc=font->GetDC();DWORD fontBytes=GetFontData(dc,0,0,nullptr,0);assert(fontBytes!=GDI_ERROR && fontBytes>1000);std::vector<std::byte> source(fontBytes);assert(GetFontData(dc,0,0,source.data(),fontBytes)==fontBytes);if(id==0)assert(source.size()==ttf.size() && std::memcmp(source.data(),ttf.data(),ttf.size())==0);Write((std::string(argv[2])+"/font"+std::to_string(id)+".sfnt").c_str(),source);TEXTMETRICW m{};assert(font->GetTextMetricsW(&m));wchar_t actual[128]{};assert(GetTextFaceW(dc,128,actual));assert(std::wstring(actual)==name);
 std::vector<wchar_t> cps(65536);std::vector<WORD> indices(65536);for(unsigned cp=0;cp<65536;++cp)cps[cp]=wchar_t(cp);
 for(unsigned cp=0;cp<65536;cp+=4096)assert(GetGlyphIndicesW(dc,cps.data()+cp,4096,indices.data()+cp,GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR);
 auto length=GetFontUnicodeRanges(dc,nullptr);assert(length);std::vector<std::byte> rangesBytes(length);auto ranges=reinterpret_cast<GLYPHSET*>(rangesBytes.data());assert(GetFontUnicodeRanges(dc,ranges));
 std::map<unsigned,unsigned> representatives;std::vector<unsigned> codes;
 for(unsigned r=0;r<ranges->cRanges;++r)for(unsigned n=0;n<ranges->ranges[r].cGlyphs;++n){unsigned cp=ranges->ranges[r].wcLow+n;
 if(cp<32 || (cp>=127 && cp<160) || indices[cp]==0xffff)continue;if(!representatives.contains(indices[cp])){representatives[indices[cp]]=unsigned(codes.size());codes.push_back(cp);}}
 std::vector<std::byte> binary;auto word=[&](unsigned v){for(unsigned k=0;k<4;++k)binary.push_back(std::byte(v>>(8*k)));};
 for(char c:std::string("MGTEXT01"))binary.push_back(std::byte(c));for(unsigned v:{id,size,unsigned(m.tmHeight),unsigned(m.tmAscent),unsigned(m.tmDescent),unsigned(codes.size())})word(v);
 for(unsigned cp=0;cp<65536;++cp){auto it=representatives.find(indices[cp]);unsigned index=it==representatives.end()?65535:it->second;binary.push_back(std::byte(index));binary.push_back(std::byte(index>>8));}
 DWORD kernCount=GetKerningPairsW(dc,0,nullptr);std::vector<KERNINGPAIR> kerns(kernCount);if(kernCount)assert(GetKerningPairsW(dc,kernCount,kerns.data())==kernCount);word(kernCount);for(auto k:kerns){word(k.wFirst);word(k.wSecond);word(unsigned(k.iKernAmount));}
 Target tile(owner.device.p,64,64);unsigned ink=0;
 for(auto cp:codes){wchar_t ch=wchar_t(cp);SIZE ext{};assert(GetTextExtentPoint32W(dc,&ch,1,&ext));assert(ext.cx<60 && m.tmHeight<60);RECT rect{2,2,64,64};tile.Begin();font->DrawTextW(nullptr,&ch,1,&rect,0x120,~0u);auto pixels=tile.End();unsigned x0=64,y0=64,x1=0,y1=0; for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)if(pixels[(y*64+x)*4]!=std::byte{}){x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x+1);y1=std::max(y1,y+1);} if(x0==64){x0=y0=x1=y1=0;}
 for(unsigned v:{cp,unsigned(ext.cx),x0,y0,x1-x0,y1-y0})word(v);
 for(unsigned y=y0;y<y1;++y)for(unsigned x=x0;x<x1;++x){auto offset=(y*64+x)*4;assert(pixels[offset]==pixels[offset+1] && pixels[offset]==pixels[offset+2]);auto alpha=pixels[offset];binary.push_back(alpha);ink+=alpha!=std::byte{};}}
 assert(ink);auto data=srhd_awa::matrixgame::text_backend::native::Data::Load(binary);
 std::vector<std::byte> golden;auto gw=[&](unsigned v){for(unsigned k=0;k<4;++k)golden.push_back(std::byte(v>>(8*k)));};
 std::vector<std::wstring> cases{L"Меню",L"AV To ijg",L"A&B &&",L" ",L"A A ",L"three",L"one two three",L"ab\r\ncd",L"",L"Желтый цвет",L"ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890",L"  one  two   three ",L"a\tb",L"a\r\nb\r\n"};
 for(auto cp:codes)cases.push_back(std::wstring(1,wchar_t(cp)));
 unsigned checked=0;
 for(auto& text:cases)for(unsigned color:(text.size()>1?std::vector<unsigned>{0xffb0ab99,0x80a7c319,0x00776655}:std::vector<unsigned>{0xffb0ab99}))for(unsigned flags:{0x100u,0x110u,0x111u,0x115u,0x11au,0x520u}){
 Target target(owner.device.p,75,20);target.Begin();Check(owner.device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0));RECT r{2,-2,73,20};int result=font->DrawTextW(nullptr,text.data(),int(text.size()),&r,flags,color);auto expected=target.End();
 srhd_awa::matrixgame::text_backend::Rect actualRect{2,-2,73,20};std::vector<std::byte> actual(75*20*4);int actualResult=srhd_awa::matrixgame::text_backend::native::Draw(*data,text.data(),int(text.size()),actualRect,flags,color,actual,75,20,75*4);
 if(actual!=expected || result!=actualResult || std::memcmp(&r,&actualRect,sizeof(r))){GCP_RESULTSW placement{};placement.lStructSize=sizeof(placement);std::vector<int> carets(text.size()),advances(text.size());placement.lpCaretPos=carets.data();placement.lpDx=advances.data();placement.nGlyphs=DWORD(text.size());GetCharacterPlacementW(dc,text.data(),int(text.size()),0,&placement,0);for(unsigned i=0;i<text.size();++i)std::printf("cp=%x caret=%d dx=%d\n",unsigned(text[i]),carets[i],advances[i]);DWORD nk=GetKerningPairsW(dc,0,nullptr);std::vector<KERNINGPAIR> pairs(nk);GetKerningPairsW(dc,nk,pairs.data());for(auto pair:pairs)if(pair.wFirst=='T' && pair.wSecond=='o')std::printf("To kern=%d\n",pair.iKernAmount);RECT wrap{2,-2,73,20};int wr=font->DrawTextW(nullptr,text.data(),int(text.size()),&wrap,0x515,0);std::printf("wrapped CALC result=%d rect=%ld,%ld,%ld,%ld\n",wr,wrap.left,wrap.top,wrap.right,wrap.bottom);RECT measure{0,0,0,0};font->DrawTextW(nullptr,L"three",5,&measure,0x520,0);SIZE ex{};GetTextExtentPoint32W(dc,L"three",5,&ex);std::printf("three calc=%ld extent=%ld\n",measure.right,ex.cx);unsigned differences=0;for(unsigned i=0;i<actual.size();++i)differences+=actual[i]!=expected[i];std::printf("MISMATCH id=%u case=%u cp=%x flags=%x result=%d/%d rect=%ld,%ld,%ld,%ld / %d,%d,%d,%d pixels=%u\n",id,checked,text.empty()?0:unsigned(text[0]),flags,result,actualResult,r.left,r.top,r.right,r.bottom,actualRect.left,actualRect.top,actualRect.right,actualRect.bottom,differences);Write((std::string(argv[2])+"/expected.bgra").c_str(),expected);Write((std::string(argv[2])+"/actual.bgra").c_str(),actual);return 11;}
 gw(unsigned(text.size()));for(auto cp:text)gw(unsigned(cp));gw(flags);gw(color);gw(unsigned(result));for(auto v:{r.left,r.top,r.right,r.bottom})gw(unsigned(v));golden.insert(golden.end(),expected.begin(),expected.end());++checked;
 }
 Write((std::string(argv[2])+"/font"+std::to_string(id)+".golden").c_str(),golden);std::printf("pixel/layout cases=%u exact\n",checked);
 Write((std::string(argv[2])+"/font"+std::to_string(id)+".bin").c_str(),binary);
 std::wprintf(L"id=%u face=%ls logical=%u actual_height=%ld ascent=%ld glyphs=%zu ink=%u\n",id,actual,size,m.tmHeight,m.tmAscent,codes.size(),ink);
 }assert(RemoveFontMemResourceEx(registration));return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 10;}}
