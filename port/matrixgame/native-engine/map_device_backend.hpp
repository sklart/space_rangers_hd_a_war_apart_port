// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "device_state_backend.hpp"
#include <cstring>
#include <type_traits>
namespace srhd_awa::matrixgame::map_device {
enum class Transform:std::uint32_t {View=2,Projection=3,World=256};
platform3g::DeviceResult SetTransform(Transform,const platform3g::WorldMatrix&);
template<class Matrix> platform3g::DeviceResult SetTransform(Transform type,const Matrix* value) {
    static_assert(std::is_trivially_copyable_v<Matrix> && sizeof(Matrix)==sizeof(platform3g::WorldMatrix));
    if(!value)throw std::invalid_argument("null map transform");
    platform3g::WorldMatrix matrix;std::memcpy(matrix.data(),value,sizeof(matrix));
    return SetTransform(type,matrix);
}
std::uint32_t RasterCaps();
std::uint32_t StencilCaps();
platform3g::DeviceResult DrawPrimitive(std::uint32_t,std::uint32_t,std::uint32_t);
platform3g::DeviceResult DrawPrimitiveUP(std::uint32_t,std::uint32_t,const void*,std::uint32_t);
platform3g::DeviceResult DrawIndexedPrimitive(std::uint32_t,int,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
void OpenManual();
bool DeviceAvailable()noexcept;
platform3g::StoredStateBackend* StateBackend();
platform3g::DeviceResult CooperativeLevel();
platform3g::DeviceResult ResetCurrentPresentation();
}
