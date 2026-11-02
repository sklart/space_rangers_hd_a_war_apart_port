// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <deko3d.hpp>
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame {
// Original D3DFILLMODE IDs. Point fill needs its own rasterization contract.
// Bind both faces every draw, so the next solid pass never inherits wireframe.
inline void ApplyLegacyPolygonMode(dk::RasterizerState& state,std::uint32_t fill){
    switch(fill){
    case 2:state.setPolygonMode(DkPolygonMode_Line);break;
    case 3:state.setPolygonMode(DkPolygonMode_Fill);break;
    default:throw std::invalid_argument("native polygon fill outside WIREFRAME/SOLID");
    }
}
}
