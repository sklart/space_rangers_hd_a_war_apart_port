// SPDX-License-Identifier: GPL-2.0-or-later
// Calls the original compiled MatrixGame manager on a real D3D9 device.
// Globals are the actual SDK types expected by that original translation unit.
#include "matrixgame_d3d9_reference.hpp"
#include "MatrixGameDll.hpp"
#include "MatrixSampleStateManager.hpp"
#include <bit>
#include <string>
IDirect3DDevice9* g_D3DD{};
D3DCAPS9 g_D3DDCaps{};
using namespace srhd_awa::matrixgame::d3d9_reference;
int main(int argc,char**argv) {
  if(argc!=2||(std::string(argv[1])!="--hal"&&std::string(argv[1])!="--ref"))return 2;
  const bool hardware=std::string(argv[1])=="--hal";
  try {
    Window window;if(!window.value)return 77;
    Owner<IDirect3D9> api;api.value=Direct3DCreate9(D3D_SDK_VERSION);if(!api.value)return 77;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.value;
    pp.BackBufferWidth=Width;pp.BackBufferHeight=Height;pp.BackBufferFormat=D3DFMT_UNKNOWN;
    pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Owner<IDirect3DDevice9> device;
    const auto hr=api->CreateDevice(0,hardware?D3DDEVTYPE_HAL:D3DDEVTYPE_REF,window.value,
      D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.put());
    if(FAILED(hr)){std::fprintf(stderr,"D3D9 device unavailable %08lx\n",unsigned long(hr));return 77;}
    g_D3DD=device.value;Check(device->GetDeviceCaps(&g_D3DDCaps),"GetDeviceCaps");
    Owner<IDirect3DTexture9> texture;
    Check(device->CreateTexture(64,64,7,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.put(),nullptr),"CreateTexture");
    for(unsigned level=0;level<7;++level) {
      D3DLOCKED_RECT lock{};Check(texture->LockRect(level,&lock,nullptr,0),"LockMip");
      for(unsigned y=0;y<(64u>>level);++y)for(unsigned x=0;x<(64u>>level);++x)
        reinterpret_cast<std::uint32_t*>(static_cast<char*>(lock.pBits)+std::size_t(y)*lock.Pitch)[x]=0xff000000u|((32+32*level)<<16);
      Check(texture->UnlockRect(level),"UnlockMip");
    }
    unsigned transitions=0;
    for(int requested:{0,1,2,3,4,8,16,32,-1}) {
      SRobotsSettings settings{};settings.m_AFDegree=requested;g_Sampler.ApplySettings(&settings);
      std::printf("settings requested=%d effective=%d cap=%lu\n",requested,g_Sampler.m_AFDegree,g_D3DDCaps.MaxAnisotropy);
      for(auto type:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})for(DWORD value:{DWORD(D3DTEXF_POINT),DWORD(D3DTEXF_LINEAR)}) {
        Check(device->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,1),"ResetAnisotropy");
        const auto result=g_Sampler.SetState(0,type,value);DWORD filter{},degree{};
        Check(device->GetSamplerState(0,type,&filter),"GetFilter");
        Check(device->GetSamplerState(0,D3DSAMP_MAXANISOTROPY,&degree),"GetAnisotropy");
        std::printf("set reg=%u input=%lu hr=%08lx stored=%lu max=%lu\n",unsigned(type),value,unsigned long(result),filter,degree);++transitions;
      }
    }
    // Draw the exact original linear -> anisotropic transformation, including
    // its undefined MIPFILTER=ANISOTROPIC, and record real pixels/validation.
    for(int requested:{0,1,2,4,8,16}) {
      SRobotsSettings settings{};settings.m_AFDegree=requested;g_Sampler.ApplySettings(&settings);
      for(auto type:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})
        Check(g_Sampler.SetState(0,type,D3DTEXF_LINEAR),"OriginalSetState");
      for(auto type:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})Check(device->SetSamplerState(0,type,D3DTADDRESS_WRAP),"Wrap");
      Check(device->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,std::bit_cast<DWORD>(.25f)),"Bias");
      const auto original=Render(device.value,texture.value,2);
      DWORD passes{};const auto validation=device->ValidateDevice(&passes);
      Check(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR),"DefinedMipLinear");
      const auto linear_mip=Render(device.value,texture.value,2);
      unsigned differences=0;for(std::size_t i=0;i<original.size();++i)differences+=original[i]!=linear_mip[i];
      std::printf("draw af=%d original=%08x linear_mip=%08x differences=%u validate_original=%08lx passes=%lu\n",
        requested,original[32*Width+32],linear_mip[32*Width+32],differences,unsigned long(validation),passes);
    }
    for(auto reg:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER}) {
      for(auto type:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER})
        Check(device->SetSamplerState(0,type,D3DTEXF_LINEAR),"ResetLinear");
      Check(device->SetSamplerState(0,D3DSAMP_MAXANISOTROPY,8),"MaxAnisotropy8");
      Check(device->SetSamplerState(0,reg,D3DTEXF_ANISOTROPIC),"IsolatedAnisotropic");
      const auto pixels=Render(device.value,texture.value,2);DWORD passes{};
      const auto result=device->ValidateDevice(&passes);
      std::printf("isolated reg=%u pixel=%08x validation=%08lx passes=%lu\n",unsigned(reg),pixels[32*Width+32],unsigned long(result),passes);
    }
    std::fprintf(stderr,"device=%s transitions=%u cap=%lu filter_caps=%08lx original_manager_executed=true draw_executed=true switch_gpu_executed=false\n",
      hardware?"HAL":"REF",transitions,g_D3DDCaps.MaxAnisotropy,g_D3DDCaps.TextureFilterCaps);
    g_D3DD=nullptr;return 0;
  }catch(const std::exception& error){g_D3DD=nullptr;std::fprintf(stderr,"anisotropy reference: %s\n",error.what());return 3;}
}
