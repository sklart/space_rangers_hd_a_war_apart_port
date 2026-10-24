// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include "platform_win32.hpp"
#endif
#include "shadow_texture_backend.hpp"
#include "buffer_resources.hpp"
#include <cstring>
#include <limits>
namespace srhd_awa::matrixgame::shadow_texture {
using namespace platform3g;
#ifdef _WIN32
namespace {
IDirect3DDevice9& ActualDevice(){
    auto* provider=dynamic_cast<D3D9Device*>(&ReadyDevice());
    if(!provider)throw Unsupported("shadow texture requires genuine D3D9 provider");
    const auto handles=provider->ReadLegacyHandles();
    if(!handles.device)throw Unsupported("shadow texture SDK identity unavailable");
    return *reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
template<class T> struct Owner {T* value{};~Owner(){if(value)value->Release();}};
struct Bindings {
    IDirect3DDevice9& device;
    Owner<IDirect3DVertexBuffer9> vertex;
    Owner<IDirect3DIndexBuffer9> index;
    UINT offset{},stride{};
    bool captured{},restored{};
    explicit Bindings(IDirect3DDevice9& d):device(d){
        CheckDeviceResult(device.GetStreamSource(0,&vertex.value,&offset,&stride),"shadow GetStreamSource");
        CheckDeviceResult(device.GetIndices(&index.value),"shadow GetIndices");captured=true;
    }
    HRESULT Restore()noexcept{
        if(!captured || restored)return D3D_OK;
        const auto a=device.SetStreamSource(0,vertex.value,offset,stride);
        const auto b=device.SetIndices(index.value);restored=true;
        return FAILED(a)?a:b;
    }
    ~Bindings(){Restore();}
};
}
#endif
DeviceResult CreateRenderTexture(std::uint32_t width,std::uint32_t height,std::uint32_t levels,
    std::uint32_t usage,std::uint32_t format,std::uint32_t pool,IDirect3DTexture9** output){
#ifdef _WIN32
    return D3DXCreateTexture(&ActualDevice(),width,height,levels,usage,
        static_cast<D3DFORMAT>(format),static_cast<D3DPOOL>(pool),output);
#else
    (void)width;(void)height;(void)levels;(void)usage;(void)format;(void)pool;(void)output;
    (void)ReadyDevice();throw Unsupported("shadow render texture SDK unavailable");
#endif
}
DeviceResult DrawIndexedStaging(std::uint32_t primitive,int base,std::uint32_t minimum,
    std::uint32_t vertices,std::uint32_t start,std::uint32_t primitives){
#ifdef _WIN32
    auto& device=ActualDevice();auto& context=buffers::Current();
    const auto binding=context.Vertex();const auto* ib=context.Index();const auto* vb=binding.buffer;
    if(primitive!=D3DPT_TRIANGLELIST || !vb || !ib || !binding.stride)
        throw std::invalid_argument("shadow draw requires selected triangle-list staging");
    const auto width=static_cast<std::uint32_t>(ib->Width());
    const std::uint64_t first=std::uint64_t(start)*width,count=std::uint64_t(primitives)*3;
    if(first>ib->Size() || count>(ib->Size()-first)/width ||
       !ib->Initialized(first,count*width) || vb->Size()>std::numeric_limits<UINT>::max() ||
       ib->Size()>std::numeric_limits<UINT>::max() || binding.offset>std::numeric_limits<UINT>::max() ||
       binding.stride>std::numeric_limits<UINT>::max())
        throw std::out_of_range("shadow staging upload/index range");
    const auto index_bytes=ib->Storage();
    for(std::uint64_t item=0;item<count;++item){
        std::uint32_t index{};
        if(width==2){std::uint16_t value;std::memcpy(&value,index_bytes.data()+first+item*width,2);index=value;}
        else std::memcpy(&index,index_bytes.data()+first+item*width,4);
        const auto absolute=std::int64_t(base)+index;
        if(index<minimum || std::uint64_t(index)-minimum>=vertices || absolute<0 ||
           std::uint64_t(absolute)>(vb->Size()/binding.stride) ||
           !vb->Initialized(binding.offset+std::uint64_t(absolute)*binding.stride,binding.stride))
            throw std::out_of_range("shadow draw references uninitialized vertex");
    }
    Bindings previous(device);Owner<IDirect3DVertexBuffer9> gpu_vb;Owner<IDirect3DIndexBuffer9> gpu_ib;
    D3DDEVICE_CREATION_PARAMETERS creation{};
    CheckDeviceResult(device.GetCreationParameters(&creation),"shadow creation flags");
    DWORD usage=D3DUSAGE_WRITEONLY;
    if(creation.BehaviorFlags&(D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MIXED_VERTEXPROCESSING))usage|=D3DUSAGE_SOFTWAREPROCESSING;
    CheckDeviceResult(device.CreateVertexBuffer(static_cast<UINT>(vb->Size()),usage,vb->Layout(),D3DPOOL_MANAGED,&gpu_vb.value,nullptr),"shadow GPU vertex allocation");
    CheckDeviceResult(device.CreateIndexBuffer(static_cast<UINT>(ib->Size()),usage,width==2?D3DFMT_INDEX16:D3DFMT_INDEX32,D3DPOOL_MANAGED,&gpu_ib.value,nullptr),"shadow GPU index allocation");
    void* data{};
    CheckDeviceResult(gpu_vb.value->Lock(0,0,&data,0),"shadow vertex upload lock");
    std::memcpy(data,vb->Storage().data(),vb->Size());
    CheckDeviceResult(gpu_vb.value->Unlock(),"shadow vertex upload unlock");
    CheckDeviceResult(gpu_ib.value->Lock(0,0,&data,0),"shadow index upload lock");
    std::memcpy(data,index_bytes.data(),ib->Size());
    CheckDeviceResult(gpu_ib.value->Unlock(),"shadow index upload unlock");
    CheckDeviceResult(device.SetStreamSource(0,gpu_vb.value,static_cast<UINT>(binding.offset),static_cast<UINT>(binding.stride)),"shadow GPU stream bind");
    CheckDeviceResult(device.SetIndices(gpu_ib.value),"shadow GPU indices bind");
    const auto result=device.DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive),base,minimum,vertices,start,primitives);
    const auto restore=previous.Restore();return FAILED(result)?result:restore;
#else
    (void)primitive;(void)base;(void)minimum;(void)vertices;(void)start;(void)primitives;
    (void)ReadyDevice();throw Unsupported("shadow staging GPU upload/draw unavailable");
#endif
}
}
