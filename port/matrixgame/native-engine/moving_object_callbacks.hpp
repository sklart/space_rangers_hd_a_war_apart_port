// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "math_types.hpp"
#include <cstdint>
class CMatrixMapStatic;
namespace srhd_awa::matrixgame::moving_object {
// Same original enum callback, not a substitute collision/trace algorithm.
bool InvokeHit(const D3DXVECTOR3&,CMatrixMapStatic*,uintptr_t properties);
}
