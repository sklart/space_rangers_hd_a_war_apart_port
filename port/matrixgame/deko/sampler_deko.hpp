// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <deko3d.hpp>
#include "sampler_state.hpp"
namespace srhd_awa::matrixgame {
// The same real SDK construction used by native draws. Anisotropy is enabled
// only by the MIN filter; MAG/MIP retain their requested point/linear behavior.
inline dk::Sampler MakeDekoSampler(const MeshSamplerState& state){
    state.Validate();
    auto filter=[](TextureFilter f){return f==TextureFilter::Point?DkFilter_Nearest:DkFilter_Linear;};
    auto wrap=[](TextureAddress a){return a==TextureAddress::Clamp?DkWrapMode_ClampToEdge:DkWrapMode_Repeat;};
    dk::Sampler sampler;
    sampler.setFilter(filter(state.min),filter(state.mag),state.mip==TextureMipFilter::None?DkMipFilter_None:state.mip==TextureMipFilter::Point?DkMipFilter_Nearest:DkMipFilter_Linear)
        .setWrapMode(wrap(state.u),wrap(state.v),wrap(state.w)).setLodBias(state.lod_bias)
        .setMaxAnisotropy(state.min==TextureFilter::Anisotropic?float(state.max_anisotropy):1.f);
    return sampler;
}
}
