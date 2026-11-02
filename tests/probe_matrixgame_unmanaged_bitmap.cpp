// SPDX-License-Identifier: GPL-2.0-or-later
// Exact original D3DX request on Windows. DEFAULT allocation/descriptors plus
// SYSTEMMEM differential pixels; no Draw/Present or Switch proof.
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
void Check(HRESULT result){if(FAILED(result)){std::fprintf(stderr,"HRESULT=%08x\n",unsigned(result));std::abort();}}
int main(int argc,char** argv){
    assert(argc==3);namespace fs=std::filesystem;
    auto window=CreateWindowExW(0,L"STATIC",L"MatrixGame unmanaged bitmap reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);
    auto* api=Direct3DCreate9(D3D_SDK_VERSION);assert(api);D3DPRESENT_PARAMETERS present{};present.Windowed=TRUE;present.SwapEffect=D3DSWAPEFFECT_DISCARD;present.hDeviceWindow=window;present.BackBufferWidth=present.BackBufferHeight=16;
    IDirect3DDevice9* device=nullptr;Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&present,&device));
    D3DCAPS9 caps{};Check(device->GetDeviceCaps(&caps));std::printf("WINDOWS HAL TextureCaps=%08x max=%ux%u\n",unsigned(caps.TextureCaps),unsigned(caps.MaxTextureWidth),unsigned(caps.MaxTextureHeight));
    const fs::path output=fs::u8path(argv[2]);fs::create_directories(output);std::ifstream manifest(fs::u8path(argv[1]));assert(manifest);std::string line;unsigned cases=0;
    while(std::getline(manifest,line)){
        const auto split=line.find('\t');assert(split!=line.npos);const auto levels=std::stoul(line.substr(0,split));const auto path=fs::u8path(line.substr(split+1));
        std::ifstream input(path,std::ios::binary);assert(input);const std::vector<char> bytes{std::istreambuf_iterator<char>(input),{}};
        IDirect3DTexture9* original=nullptr;IDirect3DTexture9* reference=nullptr;
        const auto decode=[&](D3DPOOL pool,IDirect3DTexture9** out){return D3DXCreateTextureFromFileInMemoryEx(device,bytes.data(),UINT(bytes.size()),0,0,levels,0,D3DFMT_UNKNOWN,pool,D3DX_FILTER_TRIANGLE|D3DX_FILTER_DITHER,D3DX_FILTER_BOX,0,nullptr,nullptr,out);};
        Check(decode(D3DPOOL_DEFAULT,&original));Check(decode(D3DPOOL_SYSTEMMEM,&reference));assert(original && reference && original->GetLevelCount()==reference->GetLevelCount());
        const auto name=path.stem().string()+"-L"+std::to_string(levels);std::ofstream file(output/(name+".oracle"),std::ios::binary);assert(file);
        D3DSURFACE_DESC first{};Check(original->GetLevelDesc(0,&first));
        const std::array<std::uint32_t,7> header{0x31424d55u,first.Width,first.Height,unsigned(first.Format),unsigned(first.Pool),first.Usage,original->GetLevelCount()};file.write(reinterpret_cast<const char*>(header.data()),sizeof(header));
        for(unsigned level=0;level<original->GetLevelCount();++level){
            D3DSURFACE_DESC actual{},cpu{};Check(original->GetLevelDesc(level,&actual));Check(reference->GetLevelDesc(level,&cpu));assert(actual.Width==cpu.Width && actual.Height==cpu.Height && actual.Format==cpu.Format);
            IDirect3DSurface9* source=nullptr;IDirect3DSurface9* target=nullptr;Check(reference->GetSurfaceLevel(level,&source));
            Check(device->CreateOffscreenPlainSurface(cpu.Width,cpu.Height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&target,nullptr));Check(D3DXLoadSurfaceFromSurface(target,nullptr,nullptr,source,nullptr,nullptr,D3DX_FILTER_NONE,0));
            D3DLOCKED_RECT lock{};Check(target->LockRect(&lock,nullptr,D3DLOCK_READONLY));
            const std::array<std::uint32_t,3> shape{cpu.Width,cpu.Height,cpu.Width*cpu.Height*4};file.write(reinterpret_cast<const char*>(shape.data()),sizeof(shape));
            for(unsigned y=0;y<cpu.Height;++y)file.write(static_cast<const char*>(lock.pBits)+std::size_t(y)*lock.Pitch,cpu.Width*4);
            Check(target->UnlockRect());target->Release();source->Release();
        }
        assert(file.good());std::printf("%s fmt=%u pool=%u %ux%u levels=%u\n",name.c_str(),unsigned(first.Format),unsigned(first.Pool),first.Width,first.Height,original->GetLevelCount());original->Release();reference->Release();++cases;
    }
    assert(manifest.eof());device->Release();api->Release();DestroyWindow(window);std::printf("cases=%u; DEFAULT desc and SYSTEMMEM pixels; NO_NATIVE_INIT NO_SWITCH\n",cases);
}
