// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include "material_combiner.hpp"
#include "fixed_vertex.hpp"
#include "alpha_target.hpp"
#include <cstdio>
#include <cassert>
#include <cmath>
namespace mat=srhd_awa::matrixgame::materials;
namespace m=srhd_awa::matrixgame;
#include "matrixgame_material_gl_reference.hpp"
void CheckImpl(HRESULT h,const char* call,int line){if(FAILED(h))throw std::runtime_error(std::string(call)+" line "+std::to_string(line)+" failed "+std::to_string(unsigned(h)));}
#define Check(x) CheckImpl((x),#x,__LINE__)
template<class T>struct Com {T* p{};~Com(){if(p)p->Release();}T* operator->()const{return p;}};
int main(int argc,char** argv){try{
 if(argc!=2)return 2;HWND window=CreateWindowW(L"STATIC",L"A8 HAL reference",WS_OVERLAPPEDWINDOW,0,0,256,256,0,0,GetModuleHandleW(0),0);
 Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);Com<IDirect3DDevice9> dev;D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
 Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev.p));
 Com<IDirect3DTexture9> texture,sys_texture;Com<IDirect3DSurface9> target,system;Check(dev->CreateTexture(1,1,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8,D3DPOOL_DEFAULT,&texture.p,0));Check(texture->GetSurfaceLevel(0,&target.p));Check(dev->CreateTexture(1,1,1,0,D3DFMT_A8,D3DPOOL_SYSTEMMEM,&sys_texture.p,0));Check(sys_texture->GetSurfaceLevel(0,&system.p));
 Check(dev->SetRenderTarget(0,target.p));Check(dev->SetDepthStencilSurface(0));Check(dev->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));Check(dev->SetRenderState(D3DRS_ZENABLE,FALSE));Check(dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
 Check(dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));Check(dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));Check(dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));Check(dev->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
 mat::Combiner combiner;combiner.Set(0,1,2);combiner.Set(0,2,0);combiner.Set(0,4,2);combiner.Set(0,5,0);auto u=combiner.Prepare();MaterialGlReference gl(argv[1]);unsigned count=0,largest=0;
 struct Vertex{float x,y,z,w;unsigned color;};
 for(unsigned sa:{0u,37u,129u,255u})for(unsigned da:{0u,61u,194u,255u})for(unsigned source=1;source<=11;++source)for(unsigned dest=1;dest<=10;++dest)for(unsigned op=1;op<=5;++op){
  m::MeshRasterState r;r.blend_enable=true;r.source=m::RasterBlendFactor(source);r.destination=m::RasterBlendFactor(dest);r.operation=m::RasterBlendOperation(op);r.depth_test=false;r.depth_write=false;
  Check(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE));Check(dev->SetRenderState(D3DRS_SRCBLEND,source));Check(dev->SetRenderState(D3DRS_DESTBLEND,dest));Check(dev->SetRenderState(D3DRS_BLENDOP,op));Check(dev->SetRenderState(D3DRS_COLORWRITEENABLE,15));Check(dev->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));
  Check(dev->Clear(0,0,D3DCLEAR_TARGET,(da<<24)|0x3a87d2,1,0));Vertex vertices[]={{-1,-1,0,1,(sa<<24)|0xb51a64},{3,-1,0,1,(sa<<24)|0xb51a64},{-1,3,0,1,(sa<<24)|0xb51a64}};
  Check(dev->BeginScene());Check(dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,sizeof(Vertex)));Check(dev->EndScene());Check(dev->GetRenderTargetData(target.p,system.p));D3DLOCKED_RECT lock{};Check(system->LockRect(&lock,0,D3DLOCK_READONLY));auto expected=*static_cast<unsigned char*>(lock.pBits);Check(system->UnlockRect());
  auto actual=gl.DrawAlpha(u,{181/255.f,26/255.f,100/255.f,sa/255.f},r,static_cast<unsigned char>(da));auto delta=unsigned(std::abs(int(expected)-int(actual)));largest=std::max(largest,delta);if(delta>1){std::fprintf(stderr,"A8 mismatch sa=%u da=%u src=%u dst=%u op=%u expected=%u actual=%u\n",sa,da,source,dest,op,expected,actual);return 10;}++count;
 }
 // Exercise all write masks and alpha comparisons independently of blending.
 for(unsigned mask=0;mask<16;++mask)for(unsigned compare=1;compare<=8;++compare)for(unsigned sa:{0u,128u,255u}){
  m::MeshRasterState r;r.blend_enable=false;r.alpha_test=true;r.alpha_compare=m::RasterCompare(compare);r.alpha_reference=128;r.color_write=mask;
  Check(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));Check(dev->SetRenderState(D3DRS_COLORWRITEENABLE,15));Check(dev->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE));Check(dev->SetRenderState(D3DRS_ALPHAFUNC,compare));Check(dev->SetRenderState(D3DRS_ALPHAREF,128));Check(dev->Clear(0,0,D3DCLEAR_TARGET,0x613a87d2,1,0));Check(dev->SetRenderState(D3DRS_COLORWRITEENABLE,mask));Check(dev->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE));
  Vertex v[]={{-1,-1,0,1,(sa<<24)|0xb51a64},{3,-1,0,1,(sa<<24)|0xb51a64},{-1,3,0,1,(sa<<24)|0xb51a64}};Check(dev->BeginScene());Check(dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,v,sizeof(Vertex)));Check(dev->EndScene());Check(dev->GetRenderTargetData(target.p,system.p));D3DLOCKED_RECT lock{};Check(system->LockRect(&lock,0,D3DLOCK_READONLY));auto expected=*static_cast<unsigned char*>(lock.pBits);Check(system->UnlockRect());auto actual=gl.DrawAlpha(u,{181/255.f,26/255.f,100/255.f,sa/255.f},r,97);if(expected!=actual){std::fprintf(stderr,"A8 mask/alpha mismatch mask=%u cmp=%u sa=%u expected=%u actual=%u\n",mask,compare,sa,expected,actual);return 11;}++count;
 }
 std::printf("A8 Windows HAL vs original GLSL R8 %u cases PASS maximum delta=%u: blend/write-mask/alpha-test; NO_SWITCH_GPU\n",count,largest);DestroyWindow(window);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 12;}}
