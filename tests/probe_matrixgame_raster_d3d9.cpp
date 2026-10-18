// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Windows D3D9 runtime state query. No draw or Switch GPU proof.
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <fcntl.h>
#include <io.h>
#include "../port/matrixgame/deko/raster_state.hpp"
using namespace srhd_awa::matrixgame;
static_assert(unsigned(RasterRegister::DepthTest)==D3DRS_ZENABLE && unsigned(RasterRegister::AlphaReference)==D3DRS_ALPHAREF);
static_assert(unsigned(RasterRegister::ColorWrite)==D3DRS_COLORWRITEENABLE && unsigned(RasterRegister::BlendOperation)==D3DRS_BLENDOP);
static_assert(unsigned(RasterCompare::GreaterEqual)==D3DCMP_GREATEREQUAL && unsigned(RasterCompare::Always)==D3DCMP_ALWAYS);
static_assert(unsigned(RasterBlendFactor::SourceAlpha)==D3DBLEND_SRCALPHA && unsigned(RasterBlendFactor::SourceAlphaSaturate)==D3DBLEND_SRCALPHASAT);
static_assert(unsigned(RasterBlendOperation::Maximum)==D3DBLENDOP_MAX);
int main(int argc,char** argv) {
  _setmode(_fileno(stdout),_O_BINARY);if(argc!=2)return 2;
  const auto window=CreateWindowExW(0,L"STATIC",L"MatrixGame state reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
  if(!window)return 77;
  auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api){DestroyWindow(window);return 77;}
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
  pp.BackBufferWidth=16;pp.BackBufferHeight=16;pp.BackBufferFormat=D3DFMT_UNKNOWN;
  IDirect3DDevice9* device=nullptr;
  const auto result=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_NULLREF,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device);
  if(FAILED(result)){std::fprintf(stderr,"D3D9 NULLREF unavailable hr=%08lx\n",result);api->Release();DestroyWindow(window);return 77;}
  int status=0;
  const auto finish=[&]{device->Release();api->Release();DestroyWindow(window);return status;};
  const auto initial=MeshRasterState{}.Words();
  for(std::size_t i=0;i<RasterRegisters.size();++i)
    if(FAILED(device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(RasterRegisters[i]),initial[i]))){status=3;return finish();}
  const auto dump=[&]{std::array<DWORD,11> words{};for(std::size_t i=0;i<RasterRegisters.size();++i)
    if(FAILED(device->GetRenderState(static_cast<D3DRENDERSTATETYPE>(RasterRegisters[i]),&words[i])))return false;
    return std::fwrite(words.data(),1,sizeof(words),stdout)==sizeof(words);};
  if(!dump()){status=3;return finish();}
  std::ifstream input(argv[1]);std::uint32_t reg,value;
  while(input>>reg>>value) {
    if(FAILED(device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(reg),value))||!dump()){status=3;return finish();}
  }
  if(!input.eof()||std::fflush(stdout))status=3;
  std::fprintf(stderr,"D3D9 NULLREF Set/Get only; draw_executed=false\n");return finish();
}
