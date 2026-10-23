// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#endif
#include "map_device_backend.hpp"
namespace srhd_awa::matrixgame::map_device {
using namespace platform3g;
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
DeviceResult SetTransform(Transform type,const WorldMatrix& value) {
    if(type==Transform::World) {
        return ReadyDevice().SetWorldTransform(value);
    }
    if(type!=Transform::View && type!=Transform::Projection)throw Unsupported("unknown map transform");
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
    (void)ReadyDevice();throw Unsupported("map raster caps backend unavailable");
#endif
}
std::uint32_t StencilCaps() {
#ifdef _WIN32
    return ActualCaps().StencilCaps;
#else
    (void)ReadyDevice();throw Unsupported("map stencil caps backend unavailable");
#endif
}
DeviceResult DrawPrimitive(std::uint32_t primitive,std::uint32_t start,std::uint32_t count) {
#ifdef _WIN32
    return ActualDevice().DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(primitive),start,count);
#else
    (void)primitive;(void)start;(void)count;(void)ReadyDevice();
    throw Unsupported("map draw backend unavailable");
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
