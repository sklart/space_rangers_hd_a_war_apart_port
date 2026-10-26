// Actual D3D9/D3DX reference for native BC1 outputs; no emulated GPU objects.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include "native_frame_scope.hpp"
template<class T>struct Com{T* p{};~Com(){if(p)p->Release();}T* operator->()const{return p;}};
void Check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D9/D3DX failure "+std::to_string(unsigned(hr)));}
std::vector<unsigned char> Read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);assert(f);std::vector<unsigned char> b(std::size_t(f.tellg()));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),b.size());assert(f);return b;}
std::vector<unsigned char> Pixels(IDirect3DDevice9* device,IDirect3DTexture9* texture,unsigned mip){
    Com<IDirect3DSurface9> source,target;Check(texture->GetSurfaceLevel(mip,&source.p));D3DSURFACE_DESC desc{};Check(source->GetDesc(&desc));
    Check(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&target.p,nullptr));
    Check(D3DXLoadSurfaceFromSurface(target.p,nullptr,nullptr,source.p,nullptr,nullptr,D3DX_FILTER_NONE,0));
    D3DLOCKED_RECT lr{};Check(target->LockRect(&lr,nullptr,D3DLOCK_READONLY));std::vector<unsigned char> out(std::size_t(desc.Width)*desc.Height*4);
    for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x){const auto* p=static_cast<const unsigned char*>(lr.pBits)+std::size_t(y)*lr.Pitch+x*4;auto* d=out.data()+(std::size_t(y)*desc.Width+x)*4;d[0]=p[2];d[1]=p[1];d[2]=p[0];d[3]=p[3];}Check(target->UnlockRect());return out;
}
void ClearReference(IDirect3DDevice9* device){
    namespace p=srhd_awa::matrixgame::platform3g;
    Com<IDirect3DSurface9> target,depth,readback;Check(device->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target.p,nullptr));
    Check(device->CreateDepthStencilSurface(64,64,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&depth.p,nullptr));Check(device->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback.p,nullptr));
    Check(device->SetRenderTarget(0,target.p));Check(device->SetDepthStencilSurface(depth.p));
    for(unsigned test=0;test<3;++test){D3DVIEWPORT9 full{0,0,64,64,0,1},viewport{8,12,32,24,0,1};Check(device->SetViewport(&full));Check(device->Clear(0,nullptr,7,0xff000000,1,0));Check(device->SetViewport(&viewport));
        const std::vector<p::ClearRectangle> rects=test==1?std::vector<p::ClearRectangle>{{-7,-8,24,30},{32,18,60,60}}:test==2?std::vector<p::ClearRectangle>{{-7,-8,-1,-1},{61,61,63,63}}:std::vector<p::ClearRectangle>{};
        std::vector<D3DRECT> sdk;for(auto r:rects)sdk.push_back({r.left,r.top,r.right,r.bottom});Check(device->Clear(sdk.size(),sdk.empty()?nullptr:sdk.data(),7,0xff315a89,.25f,13));
        auto plan=p::PlanNativeClear({8,12,32,24,0,1},64,64,rects,7,.25f,13);Check(device->GetRenderTargetData(target.p,readback.p));D3DLOCKED_RECT lr{};Check(readback->LockRect(&lr,nullptr,D3DLOCK_READONLY));
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){bool covered=false;for(auto r:plan)covered|=int(x)>=r.left && int(x)<r.right && int(y)>=r.top && int(y)<r.bottom;const auto* pixel=static_cast<const unsigned char*>(lr.pBits)+std::size_t(y)*lr.Pitch+x*4;unsigned value{};std::memcpy(&value,pixel,4);assert(value==(covered?0xff315a89u:0xff000000u));}Check(readback->UnlockRect());
    }
    Check(device->SetDepthStencilSurface(nullptr));std::puts("original D3D9 Clear viewport/full/partial/outside rectangles: native plan exact RGB PASS cases=3");
}
int main(int argc,char** argv){
    assert(argc==3);const std::filesystem::path input=argv[1],out=argv[2];
    HWND window=CreateWindowW(L"STATIC",L"MatrixGame BC1 reference",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);assert(api.p);Com<IDirect3DDevice9> device;D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device.p));
    unsigned cases=0,total_alpha=0,max_decoder=0;
    for(unsigned id=0;id<10;++id){const auto name=id<6?"union-"+std::to_string(id):"synthetic-"+std::to_string(id-6);const auto source=Read((id<6?input:out)/(name+".dds"));const auto native=Read(out/(name+"-native.dds"));const unsigned count=id==9?3:6;
        Com<IDirect3DTexture9> reference,encoded;Check(D3DXCreateTextureFromFileInMemoryEx(device.p,source.data(),source.size(),0,0,count,0,D3DFMT_DXT1,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_BOX,0,nullptr,nullptr,&reference.p));
        Check(D3DXCreateTextureFromFileInMemoryEx(device.p,native.data(),native.size(),0,0,count,0,D3DFMT_DXT1,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_FILTER_NONE,0,nullptr,nullptr,&encoded.p));assert(reference->GetLevelCount()==count && encoded->GetLevelCount()==count);
        Check(D3DXSaveTextureToFileA((out/(name+"-reference.dds")).string().c_str(),D3DXIFF_DDS,reference.p,nullptr));
        for(unsigned level=0;level<count;++level){auto actual=Pixels(device.p,encoded.p,level),expected=Pixels(device.p,reference.p,level),cpu=Read(out/(name+"-"+std::to_string(level)+".rgba"));assert(actual.size()==expected.size() && cpu.size()==actual.size());double sse=0;unsigned maximum=0,alpha=0,decoder=0;
            for(std::size_t p=0;p<actual.size();p+=4){if(actual[p+3]!=expected[p+3])++alpha;assert(cpu[p+3]==actual[p+3]);for(unsigned c=0;c<3;++c){const int delta=int(actual[p+c])-expected[p+c];sse+=delta*delta;maximum=std::max(maximum,unsigned(std::abs(delta)));decoder=std::max(decoder,unsigned(std::abs(int(cpu[p+c])-actual[p+c])));}}
            const auto rmse=std::sqrt(sse/(actual.size()/4*3));total_alpha+=alpha;max_decoder=std::max(max_decoder,decoder);
            std::printf("case=%s mip=%u pixels=%zu native_vs_D3DX_RMSE=%.5f max=%u alpha_mismatch=%u software_vs_D3DX_max=%u\n",name.c_str(),level,actual.size()/4,rmse,maximum,alpha,decoder);assert(decoder==0 && maximum==0 && alpha==0);
        }++cases;
    }
    ClearReference(device.p);DestroyWindow(window);std::printf("BC1 Windows HAL comparisons complete cases=%u alpha_mismatches=%u decoder_max=%u SWITCH_HARDWARE_NOT_RUN\n",cases,total_alpha,max_decoder);
}
