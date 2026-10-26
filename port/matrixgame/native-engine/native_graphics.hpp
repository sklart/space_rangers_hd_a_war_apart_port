// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <span>
#include <cstddef>
namespace srhd_awa::matrixgame::platform3g {
// Optional native interface. Does not change existing Device vtable slots or
// request a D3D identity from a native provider.
class NativeGraphics {
public:
    virtual ~NativeGraphics()=default;
    virtual DeviceResult NativeTransform(std::uint32_t,const WorldMatrix&)=0;
    virtual DeviceResult NativeDraw(std::uint32_t,std::uint32_t,std::uint32_t)=0;
    virtual DeviceResult NativeDrawUP(std::uint32_t,std::uint32_t,std::span<const std::byte>,std::uint32_t)=0;
    virtual DeviceResult NativeDrawIndexed(std::uint32_t,int,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)=0;
};
}
