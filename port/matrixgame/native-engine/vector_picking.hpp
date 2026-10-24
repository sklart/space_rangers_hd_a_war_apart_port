#pragma once
#include "math_types.hpp"
#include <algorithm>
#include <limits>

namespace srhd_awa::matrixgame::vector_picking {
// D3DXBoxBoundProbe compatibility: a ray, with no direction normalization.
// Parallel components require the origin to lie in the corresponding slab.
inline bool BoxBoundProbe(const D3DXVECTOR3* mins, const D3DXVECTOR3* maxs,
                          const D3DXVECTOR3* origin, const D3DXVECTOR3* direction) {
    float enter = 0.0f;
    float leave = std::numeric_limits<float>::infinity();
    const float lo[]{mins->x, mins->y, mins->z};
    const float hi[]{maxs->x, maxs->y, maxs->z};
    const float o[]{origin->x, origin->y, origin->z};
    const float d[]{direction->x, direction->y, direction->z};
    for (int axis = 0; axis != 3; ++axis) {
        if (d[axis] == 0.0f) {
            if (o[axis] < lo[axis] || o[axis] > hi[axis]) return false;
            continue;
        }
        float first = (lo[axis] - o[axis]) / d[axis];
        float last = (hi[axis] - o[axis]) / d[axis];
        if (first > last) std::swap(first, last);
        enter = std::max(enter, first);
        leave = std::min(leave, last);
        if (enter > leave) return false;
    }
    return true;
}
}
