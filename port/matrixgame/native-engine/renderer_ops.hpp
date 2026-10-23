// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
// Original ordinary 3g.hpp helpers, shared by map, debug text and renderer.
void SetColorOp(int,std::uint32_t,std::uint32_t,std::uint32_t);
void SetAlphaOp(int,std::uint32_t,std::uint32_t,std::uint32_t);
void SetColorOpAnyOrder(int,std::uint32_t,std::uint32_t,std::uint32_t);
void SetAlphaOpAnyOrder(int,std::uint32_t,std::uint32_t,std::uint32_t);
void SetColorOpSelect(int,std::uint32_t);
void SetAlphaOpSelect(int,std::uint32_t);
void SetColorOpDisable(int);
void SetAlphaOpDisable(int);
namespace original_renderer_ops {
using ::SetColorOp;using ::SetAlphaOp;
using ::SetColorOpAnyOrder;using ::SetAlphaOpAnyOrder;
using ::SetColorOpSelect;using ::SetAlphaOpSelect;
using ::SetColorOpDisable;using ::SetAlphaOpDisable;
}
