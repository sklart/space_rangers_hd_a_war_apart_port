// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
namespace srhd_awa::matrixgame::platform3g {
// Separate native provider: no COM identity and no expansion of Device's ABI.
class NativeMapQueries {
public:
    virtual ~NativeMapQueries()=default;
    virtual VideoViewport NativeReadViewport()const=0;
    virtual DeviceResult NativeCooperativeLevel()const=0;
    virtual std::uint32_t NativeRasterCaps()const{throw Unsupported("map raster caps backend unavailable");}
    virtual std::uint32_t NativeStencilCaps()const{throw Unsupported("map stencil caps backend unavailable");}
};
}
