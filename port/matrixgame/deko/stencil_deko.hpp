// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "stencil_state.hpp"
#include <deko3d.hpp>
namespace srhd_awa::matrixgame {
static_assert(DkStencilOp_Keep==1 && DkStencilOp_Zero==2 && DkStencilOp_Replace==3);
static_assert(DkStencilOp_Incr==4 && DkStencilOp_Decr==5 && DkStencilOp_Invert==6);
static_assert(DkStencilOp_IncrWrap==7 && DkStencilOp_DecrWrap==8);
inline void PrepareStencil(dk::CmdBuf commands,dk::DepthStencilState& depth,const MeshStencilState& state){
    state.Validate();const auto& front=state.Face(true);const auto& back=state.Face(false);
    depth.setStencilTestEnable(state.enabled)
        .setStencilFrontFailOp(static_cast<DkStencilOp>(front.fail)).setStencilFrontDepthFailOp(static_cast<DkStencilOp>(front.depth_fail))
        .setStencilFrontPassOp(static_cast<DkStencilOp>(front.pass)).setStencilFrontCompareOp(static_cast<DkCompareOp>(front.compare))
        .setStencilBackFailOp(static_cast<DkStencilOp>(back.fail)).setStencilBackDepthFailOp(static_cast<DkStencilOp>(back.depth_fail))
        .setStencilBackPassOp(static_cast<DkStencilOp>(back.pass)).setStencilBackCompareOp(static_cast<DkCompareOp>(back.compare));
    commands.setStencil(DkFace_FrontAndBack,std::uint8_t(state.write_mask),state.Reference8(),std::uint8_t(state.read_mask));
}
}
