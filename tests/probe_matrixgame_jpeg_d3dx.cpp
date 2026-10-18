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
  _setmode(_fileno(stdout),_O_BINARY);if(argc!=2)return 2;
  const auto window=CreateWindowExW(0,L"STATIC",L"MatrixGame JPEG reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  if(!window)return 77;
  auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api){DestroyWindow(window);return 77;}
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
  pp.BackBufferWidth=16;pp.BackBufferHeight=16;IDirect3DDevice9* device=nullptr;
  if(FAILED(api->CreateDevice(0,D3DDEVTYPE_NULLREF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device))){api->Release();DestroyWindow(window);return 77;}
  int status=0;std::ifstream manifest(std::filesystem::u8path(argv[1]));std::string name;
  while(std::getline(manifest,name)) {
    std::ifstream file(std::filesystem::u8path(name),std::ios::binary);
    const std::vector<char> source{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
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
