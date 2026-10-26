// Actual D3D9 HAL vs BOTH production GLSL stages; no native GPU claim.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "fixed_vertex.hpp"
#include "pixel_fog.hpp"
#include "raster_state.hpp"
#include <cassert>
#include <bit>
#include <cstring>
#include <cstdio>
#include <fstream>
namespace mat=srhd_awa::matrixgame::materials;
namespace p=srhd_awa::matrixgame::platform3g;
#include "matrixgame_material_gl_reference.hpp"
template<class T>struct Owner{T* value{};~Owner(){if(value)value->Release();}T** Put(){assert(!value);return &value;}T* operator->()const{return value;}};
void HR(HRESULT result){if(FAILED(result)){std::printf("HRESULT=%08x\n",unsigned(result));throw std::runtime_error("actual HAL fog operation failed");}}
struct Hal {
    unsigned width,height;
    HWND window{};Owner<IDirect3D9> api;Owner<IDirect3DDevice9> device;
    Owner<IDirect3DSurface9> target,readback,depth;
    explicit Hal(unsigned w=16,unsigned h=16):width(w),height(h){window=CreateWindowW(L"STATIC",L"Original pixel fog HAL",WS_OVERLAPPEDWINDOW,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);
        api.value=Direct3DCreate9(D3D_SDK_VERSION);assert(api.value);D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=16;pp.BackBufferHeight=16;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
        HR(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.Put()));
        D3DCAPS9 caps{};HR(device->GetDeviceCaps(&caps));assert((caps.RasterCaps&mat::PixelFogCaps)==mat::PixelFogCaps);
        D3DADAPTER_IDENTIFIER9 id{};HR(api->GetAdapterIdentifier(0,0,&id));std::printf("actual HAL adapter=%s RasterCaps=%08x\n",id.Description,unsigned(caps.RasterCaps));
        HR(device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,target.Put(),nullptr));HR(device->CreateOffscreenPlainSurface(width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,readback.Put(),nullptr));HR(device->SetRenderTarget(0,target.value));
        HR(device->CreateDepthStencilSurface(width,height,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,depth.Put(),nullptr));HR(device->SetDepthStencilSurface(depth.value));
    }
    ~Hal(){if(window)DestroyWindow(window);}
    std::vector<unsigned> Draw(unsigned fvf,std::span<const std::byte> bytes,unsigned stride,const mat::Matrix& projection,const mat::PixelFogUniforms& fog,const srhd_awa::matrixgame::MeshRasterState& raster,const MaterialMeshOptions& options={},const mat::Uniforms* material=nullptr,const D3DVIEWPORT9* custom_viewport=nullptr){
        const auto identity=mat::Identity();for(auto pair:{std::pair{D3DTS_WORLD,identity},std::pair{D3DTS_VIEW,identity},std::pair{D3DTS_PROJECTION,projection}}){D3DMATRIX m;std::memcpy(&m,pair.second.data(),64);HR(device->SetTransform(pair.first,&m));}
        const D3DVIEWPORT9 vp{0,0,width,height,0,1};HR(device->SetViewport(&vp));
        for(auto state:{std::pair{D3DRS_LIGHTING,0u},std::pair{D3DRS_SPECULARENABLE,0u},std::pair{D3DRS_ZENABLE,0u},std::pair{D3DRS_CULLMODE,1u},std::pair{D3DRS_ALPHABLENDENABLE,unsigned(raster.blend_enable)},std::pair{D3DRS_SRCBLEND,unsigned(raster.source)},std::pair{D3DRS_DESTBLEND,unsigned(raster.destination)},std::pair{D3DRS_BLENDOP,unsigned(raster.operation)},std::pair{D3DRS_ALPHATESTENABLE,unsigned(raster.alpha_test)},std::pair{D3DRS_ALPHAREF,raster.alpha_reference},std::pair{D3DRS_ALPHAFUNC,unsigned(raster.alpha_compare)},std::pair{D3DRS_FOGENABLE,fog.controls[0]},std::pair{D3DRS_FOGTABLEMODE,fog.controls[1]},std::pair{D3DRS_FOGVERTEXMODE,0u},std::pair{D3DRS_RANGEFOGENABLE,0u}})HR(device->SetRenderState(state.first,state.second));
        auto byte=[](float v){return unsigned(std::lround(v*255));};const auto c=fog.color;HR(device->SetRenderState(D3DRS_FOGCOLOR,(byte(c[3])<<24)|(byte(c[0])<<16)|(byte(c[1])<<8)|byte(c[2])));
        HR(device->SetRenderState(D3DRS_FOGSTART,std::bit_cast<unsigned>(fog.parameters[0])));HR(device->SetRenderState(D3DRS_FOGEND,std::bit_cast<unsigned>(fog.parameters[1])));HR(device->SetRenderState(D3DRS_FOGDENSITY,std::bit_cast<unsigned>(fog.parameters[2])));
        HR(device->SetRenderState(D3DRS_SHADEMODE,options.shade));HR(device->SetRenderState(D3DRS_CULLMODE,options.cull));HR(device->SetRenderState(D3DRS_ZENABLE,raster.depth_test));HR(device->SetRenderState(D3DRS_ZWRITEENABLE,raster.depth_write));HR(device->SetRenderState(D3DRS_ZFUNC,unsigned(raster.depth_compare)));HR(device->SetRenderState(D3DRS_COLORWRITEENABLE,raster.color_write));
        const auto& st=options.stencil;
        for(auto pair:{std::pair{D3DRS_STENCILENABLE,unsigned(st.enabled)},std::pair{D3DRS_TWOSIDEDSTENCILMODE,unsigned(st.two_sided)},std::pair{D3DRS_STENCILREF,st.reference},std::pair{D3DRS_STENCILMASK,st.read_mask},std::pair{D3DRS_STENCILWRITEMASK,st.write_mask},std::pair{D3DRS_STENCILFAIL,st.clockwise.fail},std::pair{D3DRS_STENCILZFAIL,st.clockwise.depth_fail},std::pair{D3DRS_STENCILPASS,st.clockwise.pass},std::pair{D3DRS_STENCILFUNC,st.clockwise.compare},std::pair{D3DRS_CCW_STENCILFAIL,st.counterclockwise.fail},std::pair{D3DRS_CCW_STENCILZFAIL,st.counterclockwise.depth_fail},std::pair{D3DRS_CCW_STENCILPASS,st.counterclockwise.pass},std::pair{D3DRS_CCW_STENCILFUNC,st.counterclockwise.compare}})HR(device->SetRenderState(pair.first,pair.second));
        for(unsigned i=0;i<8;++i){HR(device->SetTexture(i,nullptr));HR(device->SetTextureStageState(i,D3DTSS_COLOROP,i?D3DTOP_DISABLE:D3DTOP_SELECTARG1));HR(device->SetTextureStageState(i,D3DTSS_COLORARG1,D3DTA_DIFFUSE));HR(device->SetTextureStageState(i,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));HR(device->SetTextureStageState(i,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));}
        if(material){const auto f=material->factor;HR(device->SetRenderState(D3DRS_TEXTUREFACTOR,(byte(f[3])<<24)|(byte(f[0])<<16)|(byte(f[1])<<8)|byte(f[2])));
            for(unsigned i=0;i<8;++i){const auto& s=material->stages[i];for(unsigned r=1;r<=6;++r)HR(device->SetTextureStageState(i,D3DTEXTURESTAGESTATETYPE(r),r<=3?s.color[r-1]:s.alpha[r-4]));HR(device->SetTextureStageState(i,D3DTSS_RESULTARG,s.color[3]));}}
        HR(device->SetFVF(fvf));if(!options.preserve)HR(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0x70492795,options.clear_depth,options.clear_stencil));
        HR(device->SetRenderState(D3DRS_LASTPIXEL,options.last_pixel));
        if(custom_viewport)HR(device->SetViewport(custom_viewport));
        const auto count=unsigned(bytes.size()/stride);const auto primitives=options.primitive==1?count:options.primitive==2?count/2:options.primitive==3?count-1:options.primitive==4?count/3:count-2;
        HR(device->BeginScene());HR(device->DrawPrimitiveUP(D3DPRIMITIVETYPE(options.primitive),primitives,bytes.data(),stride));HR(device->EndScene());return ReadColor();
    }
    std::vector<unsigned> ReadColor(){HR(device->GetRenderTargetData(target.value,readback.value));
        D3DLOCKED_RECT lock;HR(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));std::vector<unsigned> out(width*height);for(unsigned y=0;y<height;++y)std::memcpy(out.data()+y*width,static_cast<const std::byte*>(lock.pBits)+y*lock.Pitch,width*4);HR(readback->UnlockRect());return out;
    }
    std::vector<unsigned char> ReadStencil(){
        for(auto pair:{std::pair{D3DRS_ZENABLE,0u},std::pair{D3DRS_LIGHTING,0u},std::pair{D3DRS_FOGENABLE,0u},std::pair{D3DRS_CULLMODE,1u},std::pair{D3DRS_ALPHATESTENABLE,0u},std::pair{D3DRS_ALPHABLENDENABLE,1u},std::pair{D3DRS_SRCBLEND,2u},std::pair{D3DRS_DESTBLEND,2u},std::pair{D3DRS_BLENDOP,1u},std::pair{D3DRS_COLORWRITEENABLE,15u},std::pair{D3DRS_STENCILENABLE,1u},std::pair{D3DRS_TWOSIDEDSTENCILMODE,0u},std::pair{D3DRS_STENCILFUNC,6u},std::pair{D3DRS_STENCILREF,0u},std::pair{D3DRS_STENCILWRITEMASK,0u},std::pair{D3DRS_STENCILFAIL,1u},std::pair{D3DRS_STENCILZFAIL,1u},std::pair{D3DRS_STENCILPASS,1u}})HR(device->SetRenderState(pair.first,pair.second));
        HR(device->SetTextureStageState(0,D3DTSS_COLOROP,2));HR(device->SetTextureStageState(0,D3DTSS_COLORARG1,3));HR(device->SetTextureStageState(0,D3DTSS_ALPHAOP,2));HR(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,3));HR(device->SetTextureStageState(1,D3DTSS_COLOROP,1));
        struct V{float x,y,z,w;unsigned color;};const V quad[]{{-.5f,-.5f,0,1,0},{15.5f,-.5f,0,1,0},{-.5f,15.5f,0,1,0},{15.5f,15.5f,0,1,0}};HR(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));HR(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0));
        HR(device->BeginScene());for(unsigned bit=1;bit<256;bit<<=1){HR(device->SetRenderState(D3DRS_STENCILMASK,bit));HR(device->SetRenderState(D3DRS_TEXTUREFACTOR,bit<<16));HR(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(V)));}HR(device->EndScene());
        const auto colors=ReadColor();std::vector<unsigned char> result(256);for(unsigned i=0;i<256;++i)result[i]=(colors[i]>>16)&255;return result;
    }
};
struct Golden {unsigned mode,w_depth,enabled,source,fog;float start,end,density,depth;unsigned result;};
static_assert(sizeof(Golden)==40);
#ifndef MATRIXGAME_FOG_DRIVER_ONLY
int main(int argc,char** argv){assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);Hal hal;MaterialGlReference gl(argv[1]);std::ofstream golden(argv[2],std::ios::binary);assert(golden);
    unsigned cases=0,pixels=0,mismatches=0,max_delta=0,gradient_cases=0;mat::Combiner combiner;combiner.Set(0,1,2);combiner.Set(0,2,0);combiner.Set(0,4,2);combiner.Set(0,5,0);const auto material=combiner.Prepare();
    for(bool source_range:{false,true})for(bool gradient:{false,true})for(bool perspective:{false,true})for(bool transformed:{false,true})for(unsigned mode:{1u,2u,3u})for(unsigned option=0;option<7;++option)for(unsigned di=0;di<(gradient?1u:7u);++di){
        const float depth=perspective?(source_range?std::array<float,7>{100,1000,2000,2400,2800,4000,8000}[di]:std::array<float,7>{.2f,1,4,8,12,20,60}[di]):(source_range?std::array<float,7>{0,.3f,.5f,.6f,.7f,.9f,1}[di]:std::array<float,7>{0,.1f,.25f,.5f,.75f,.9f,1}[di]);
        const mat::Matrix projection=perspective?mat::Matrix{1,0,0,0,0,1,0,0,0,0,10000.f/9999.9f,1,0,0,-1000.f/9999.9f,0}:mat::Identity();
        const unsigned source=option==0?0x003171b3u:option==1?0xff3171b3u:0x813171b3u,fog_argb=(option==3 || option==4)?0xfec35127u:0x01c35127u;
        // Original MatrixCamera uses maxDistance * FOG_NEAR_K(.5) / FOG_FAR_K(.7).
        const float start=perspective?(source_range?2000.f:4.f):(source_range?.5f:.25f),end=perspective?(source_range?2800.f:16.f):(source_range?.7f:.75f),density=perspective?(source_range?.0005f:.1f):.7f;
        auto fog=mat::PreparePixelFog(option!=6,mode,start,end,density,fog_argb,projection);
        srhd_awa::matrixgame::MeshRasterState raster;raster.depth_test=false;raster.blend_enable=option==2;raster.alpha_test=option==3 || option==5;raster.alpha_reference=option==5?128:130;
        const unsigned fvf=transformed?0xc4:0xc2;mat::FixedVertexState vertex;vertex.Layout(fvf);vertex.Viewport({0,0,16,16,0,1});vertex.Transform(3,projection);
        std::vector<std::byte> raw;auto append=[&](auto value){const auto* p=reinterpret_cast<const std::byte*>(&value);raw.insert(raw.end(),p,p+sizeof(value));};
        const std::array<unsigned,6> corners{0,1,2,2,1,3};
        for(auto corner:corners){const float sx=corner&1?24.f:-8.f,sy=corner&2?24.f:-8.f;
            const float d=gradient?(perspective?(source_range?std::array<float,4>{500,3000,1500,8000}[corner]:std::array<float,4>{1,40,10,90}[corner]):std::array<float,4>{.1f,.3f,.7f,.9f}[corner]):depth;
            if(transformed){append(sx);append(sy);append(perspective?.6f:d);append(perspective?1/d:1.f);}
            else {append((2*sx/16-1)*(perspective?d:1));append((1-2*sy/16)*(perspective?d:1));append(d);}
            append(source);append(0x3affffffu);
        }
        const auto vertices=vertex.Decode(raw,vertex.Stride());const auto sdk=hal.Draw(fvf,raw,vertex.Stride(),projection,fog,raster);const auto shader=gl.DrawMesh(vertex.Prepare(),material,fog,vertices,raster);
        unsigned case_delta=0;for(unsigned y=2;y<14;++y)for(unsigned x=2;x<14;++x){++pixels;for(unsigned c=0;c<4;++c){const int a=(sdk[y*16+x]>>(c*8))&255,b=(shader[y*16+x]>>(c*8))&255;case_delta=std::max(case_delta,unsigned(std::abs(a-b)));}}
        max_delta=std::max(max_delta,case_delta);if(case_delta>2){if(mismatches<12)std::printf("fog mismatch gradient=%u perspective=%u TL=%u mode=%u option=%u depth=%f delta=%u HAL=%08x GLSL=%08x\n",gradient,perspective,transformed,mode,option,depth,case_delta,sdk[8*16+8],shader[8*16+8]);++mismatches;}
        if(!gradient && (option<2 || option>=4)){Golden g{fog.controls[1],fog.controls[2],fog.controls[0],source,fog_argb,start,end,density,depth,sdk[8*16+8]};golden.write(reinterpret_cast<const char*>(&g),sizeof(g));}
        ++cases;gradient_cases+=gradient;
    }
    std::printf("pixel fog actual HAL/production GLSL cases=%u gradients=%u sampled_pixels=%u mismatches=%u max_delta=%u SWITCH_HARDWARE_NOT_RUN\n",cases,gradient_cases,pixels,mismatches,max_delta);return mismatches?1:0;
}
#endif
