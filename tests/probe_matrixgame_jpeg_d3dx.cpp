// SPDX-License-Identifier: GPL-2.0-or-later
// The actual original D3DX image decoder into a real SYSTEMMEM surface.
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <fcntl.h>
#include <io.h>
int main(int argc,char** argv) {
  _setmode(_fileno(stdout),_O_BINARY);const bool mip_info=argc==3&&std::string(argv[1])=="--mip-info";
  const bool mip_data=argc==3&&std::string(argv[1])=="--mip-data";
  if(argc!=2&&!mip_info&&!mip_data)return 2;
  const auto window=CreateWindowExW(0,L"STATIC",L"MatrixGame JPEG reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  if(!window)return 77;
  auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api){DestroyWindow(window);return 77;}
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
  pp.BackBufferWidth=16;pp.BackBufferHeight=16;IDirect3DDevice9* device=nullptr;
  if(FAILED(api->CreateDevice(0,D3DDEVTYPE_NULLREF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device))){api->Release();DestroyWindow(window);return 77;}
  int status=0;std::ifstream manifest(std::filesystem::u8path(argv[(mip_info||mip_data)?2:1]));std::string name;
  while(std::getline(manifest,name)) {
    std::ifstream file(std::filesystem::u8path(name),std::ios::binary);
    const std::vector<char> source{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
    if(mip_info||mip_data) {
      IDirect3DTexture9* texture=nullptr;
      if(FAILED(D3DXCreateTextureFromFileInMemoryEx(device,source.data(),source.size(),0,0,0,0,D3DFMT_UNKNOWN,
          D3DPOOL_SYSTEMMEM,D3DX_FILTER_NONE,D3DX_FILTER_BOX,0,nullptr,nullptr,&texture))){status=3;break;}
      D3DSURFACE_DESC desc{};texture->GetLevelDesc(0,&desc);
      const std::array<std::uint32_t,4> header{desc.Width,desc.Height,texture->GetLevelCount(),desc.Format};
      if(mip_info){std::fwrite(header.data(),1,sizeof(header),stdout);texture->Release();continue;}
      const unsigned kind=desc.Format==D3DFMT_DXT1?1:desc.Format==D3DFMT_DXT3?2:desc.Format==D3DFMT_DXT5?3:0;
      const std::array<std::uint32_t,2> summary{kind,texture->GetLevelCount()};std::fwrite(summary.data(),1,sizeof(summary),stdout);
      for(unsigned level=0;level<summary[1];++level) {
        texture->GetLevelDesc(level,&desc);IDirect3DSurface9* original=nullptr;IDirect3DSurface9* target=nullptr;
        if(FAILED(texture->GetSurfaceLevel(level,&original))){status=3;break;}
        if(kind){target=original;target->AddRef();}
        else if(FAILED(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&target,nullptr))||
            FAILED(D3DXLoadSurfaceFromSurface(target,nullptr,nullptr,original,nullptr,nullptr,D3DX_FILTER_NONE,0))) {
          if(target)target->Release();original->Release();status=3;break;
        }
        D3DLOCKED_RECT locked{};
        if(FAILED(target->LockRect(&locked,nullptr,D3DLOCK_READONLY))){target->Release();original->Release();status=3;break;}
        const auto pitch=kind?((desc.Width+3)/4)*(kind==1?8:16):desc.Width*4,rows=kind?(desc.Height+3)/4:desc.Height;
        const std::array<std::uint32_t,3> shape{desc.Width,desc.Height,pitch*rows};std::fwrite(shape.data(),1,sizeof(shape),stdout);
        std::vector<unsigned char> row(pitch);
        for(unsigned y=0;y<rows;++y) {
          const auto* src=static_cast<const unsigned char*>(locked.pBits)+std::size_t(y)*locked.Pitch;
          if(kind)std::fwrite(src,1,pitch,stdout);
          else {for(unsigned x=0;x<desc.Width;++x){row[x*4]=src[x*4+2];row[x*4+1]=src[x*4+1];row[x*4+2]=src[x*4];row[x*4+3]=src[x*4+3];}std::fwrite(row.data(),1,pitch,stdout);}
        }
        target->UnlockRect();target->Release();original->Release();
      }
      texture->Release();if(status)break;continue;
    }
    D3DXIMAGE_INFO info{};IDirect3DSurface9* surface=nullptr;D3DLOCKED_RECT lock{};
    if(source.empty()||FAILED(D3DXGetImageInfoFromFileInMemory(source.data(),source.size(),&info))||
       FAILED(device->CreateOffscreenPlainSurface(info.Width,info.Height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&surface,nullptr))){status=3;break;}
    if(FAILED(D3DXLoadSurfaceFromFileInMemory(surface,nullptr,nullptr,source.data(),source.size(),nullptr,D3DX_FILTER_NONE,0,nullptr))||
       FAILED(surface->LockRect(&lock,nullptr,D3DLOCK_READONLY))){surface->Release();status=3;break;}
    const std::array<std::uint32_t,2> header{info.Width,info.Height};std::fwrite(header.data(),1,sizeof(header),stdout);
    std::vector<unsigned char> rgba(std::size_t(info.Width)*4);
    for(unsigned y=0;y<info.Height;++y) {
      const auto* row=static_cast<const unsigned char*>(lock.pBits)+std::size_t(y)*lock.Pitch;
      for(unsigned x=0;x<info.Width;++x){rgba[x*4]=row[x*4+2];rgba[x*4+1]=row[x*4+1];rgba[x*4+2]=row[x*4];rgba[x*4+3]=row[x*4+3];}
      if(std::fwrite(rgba.data(),1,rgba.size(),stdout)!=rgba.size()){status=3;break;}
    }
    surface->UnlockRect();surface->Release();if(status)break;
  }
  if(!manifest.eof()||std::fflush(stdout))status=3;
  device->Release();api->Release();DestroyWindow(window);return status;
}
