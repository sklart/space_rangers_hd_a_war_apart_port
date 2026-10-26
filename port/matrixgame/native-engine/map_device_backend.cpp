// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#endif
#include "map_device_backend.hpp"
#include "staging_draw.hpp"
#include "native_graphics.hpp"
#include "native_map_queries.hpp"
namespace srhd_awa::matrixgame::map_device {
using namespace platform3g;
bool DeviceAvailable()noexcept{return Current().ready && Current().device;}
StoredStateBackend* StateBackend(){
    auto* backend=dynamic_cast<StoredStateBackend*>(&ReadyDevice());
    if(!backend)throw Unsupported("map stored device state backend unavailable");
    return backend;
}
DeviceResult ResetCurrentPresentation(){return ReadyDevice().ResetPresentation(Current().presentation);}
#ifdef _WIN32
namespace {
IDirect3DDevice9& ActualDevice() {
    // Only the concrete borrowed SDK provider can supply a COM identity.
    auto* provider=dynamic_cast<D3D9Device*>(&ReadyDevice());
    if(!provider)throw Unsupported("map SDK operation requires the actual D3D9 provider");
    const auto handles=provider->ReadLegacyHandles();
    if(!handles.device)throw Unsupported("map SDK device identity unavailable");
    return *reinterpret_cast<IDirect3DDevice9*>(handles.device);
}
D3DCAPS9 ActualCaps() {
    D3DCAPS9 caps{};CheckDeviceResult(ActualDevice().GetDeviceCaps(&caps),"map GetDeviceCaps");return caps;
}
}
#endif
DeviceResult CooperativeLevel(){
#ifdef _WIN32
    return ActualDevice().TestCooperativeLevel();
#else
    auto* provider=dynamic_cast<NativeMapQueries*>(&ReadyDevice());
    if(!provider)throw Unsupported("map native cooperative state provider unavailable");
    return provider->NativeCooperativeLevel();
#endif
}
DeviceResult SetTransform(Transform type,const WorldMatrix& value) {
    if(type==Transform::World) {
        return ReadyDevice().SetWorldTransform(value);
    }
    if(type!=Transform::View && type!=Transform::Projection)throw Unsupported("unknown map transform");
    if(auto* native=dynamic_cast<NativeGraphics*>(&ReadyDevice()))return native->NativeTransform(static_cast<std::uint32_t>(type),value);
#ifdef _WIN32
    D3DMATRIX matrix;static_assert(sizeof(matrix)==sizeof(value));std::memcpy(&matrix,value.data(),sizeof(matrix));
    return ActualDevice().SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(type),&matrix);
#else
    (void)ReadyDevice();throw Unsupported("map view/projection transform backend unavailable");
#endif
}
std::uint32_t RasterCaps() {
#ifdef _WIN32
    return ActualCaps().RasterCaps;
#else
    auto* provider=dynamic_cast<NativeMapQueries*>(&ReadyDevice());
    if(!provider)throw Unsupported("map raster caps backend unavailable");
    return provider->NativeRasterCaps();
#endif
}
std::uint32_t StencilCaps() {
#ifdef _WIN32
    return ActualCaps().StencilCaps;
#else
    auto* provider=dynamic_cast<NativeMapQueries*>(&ReadyDevice());
    if(!provider)throw Unsupported("map stencil caps backend unavailable");
    return provider->NativeStencilCaps();
#endif
}
DeviceResult DrawPrimitive(std::uint32_t primitive,std::uint32_t start,std::uint32_t count) {
    if(auto* native=dynamic_cast<NativeGraphics*>(&ReadyDevice()))return native->NativeDraw(primitive,start,count);
#ifdef _WIN32
    const auto binding=buffers::Current().Vertex();
    const auto elements=staging_draw::ElementCount(primitive,count);
    for(std::uint64_t i=0;i<elements;++i)staging_draw::Vertex(binding,std::uint64_t(start)+i);
    CheckDeviceResult(ReadyDevice().BindStagingVertex(0,const_cast<buffers::VertexBuffer*>(binding.buffer),binding.offset,binding.stride),"map vertex staging binding");
    const auto result=ActualDevice().DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive),start,count);
    if(SUCCEEDED(result))dynamic_cast<D3D9Device&>(ReadyDevice()).RecordDraw(false,count,primitive);
    return result;
#else
    (void)primitive;(void)start;(void)count;(void)ReadyDevice();
    throw Unsupported("map draw backend unavailable");
#endif
}
DeviceResult DrawPrimitiveUP(std::uint32_t primitive,std::uint32_t count,const void* data,std::uint32_t stride) {
    if(auto* native=dynamic_cast<NativeGraphics*>(&ReadyDevice())){
        const auto elements=staging_draw::ElementCount(primitive,count);
        if(!stride || elements>std::numeric_limits<std::size_t>::max()/stride || (elements && !data))throw std::invalid_argument("native immediate source extent");
        return native->NativeDrawUP(primitive,count,{static_cast<const std::byte*>(data),std::size_t(elements)*stride},stride);
    }
#ifdef _WIN32
    const auto result=ActualDevice().DrawPrimitiveUP(static_cast<D3DPRIMITIVETYPE>(primitive),count,data,stride);
    if(SUCCEEDED(result))dynamic_cast<D3D9Device&>(ReadyDevice()).RecordDraw(false,count,primitive,data,stride);
    return result;
#else
    (void)primitive;(void)count;(void)data;(void)stride;(void)ReadyDevice();
    throw Unsupported("map immediate draw backend unavailable");
#endif
}
DeviceResult DrawIndexedPrimitive(std::uint32_t primitive,int base_vertex,std::uint32_t minimum_vertex,
                                 std::uint32_t vertices,std::uint32_t start_index,std::uint32_t primitives) {
    if(auto* native=dynamic_cast<NativeGraphics*>(&ReadyDevice()))return native->NativeDrawIndexed(primitive,base_vertex,minimum_vertex,vertices,start_index,primitives);
#ifdef _WIN32
    auto& context=buffers::Current();
    staging_draw::ValidateIndexed(context,primitive,base_vertex,minimum_vertex,vertices,start_index,primitives);
    const auto binding=context.Vertex();
    CheckDeviceResult(ReadyDevice().BindStagingVertex(0,const_cast<buffers::VertexBuffer*>(binding.buffer),binding.offset,binding.stride),"map vertex staging binding");
    CheckDeviceResult(ReadyDevice().BindStagingIndex(const_cast<buffers::IndexBuffer*>(context.Index())),"map index staging binding");
    const auto result=ActualDevice().DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive),base_vertex,
                                              minimum_vertex,vertices,start_index,primitives);
    if(SUCCEEDED(result))dynamic_cast<D3D9Device&>(ReadyDevice()).RecordDraw(true,primitives,primitive);
    return result;
#else
    (void)primitive;(void)base_vertex;(void)minimum_vertex;(void)vertices;(void)start_index;(void)primitives;
    (void)ReadyDevice();throw Unsupported("map indexed draw backend unavailable");
#endif
}
void OpenManual() {
#ifdef _WIN32
    PROCESS_INFORMATION process{};STARTUPINFOA startup{};startup.cb=sizeof(startup);
    // Original Manual.exe/current-directory launch, only after the caller's
    // real CFile::FileExist check. Never invoked by the bootstrap tests.
    CreateProcessA("Manual.exe",nullptr,nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process);
    struct Handles {PROCESS_INFORMATION& p;~Handles(){if(p.hProcess)CloseHandle(p.hProcess);if(p.hThread)CloseHandle(p.hThread);}} handles{process};
    ReadyWindow().Show(Visibility::Minimized);
#else
    throw Unsupported("external Windows manual launch unavailable");
#endif
}
}
