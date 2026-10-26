// SPDX-License-Identifier: GPL-2.0-or-later
// Actual production native Texture/StateStore ownership, CPU image fixture.
#include "native_texture.hpp"
#include "native_device_state.hpp"
#include <cassert>
#include <cstdio>
namespace n=srhd_awa::matrixgame::native_texture;
namespace p=srhd_awa::matrixgame::platform3g;
struct CpuImage final:n::Image{};
int main(){
    auto session=std::make_shared<n::Session>();session->upload=[](const n::Texture&){return std::make_shared<CpuImage>();};
    p::NativeStateStore state;IDirect3DTexture9* texture{};n::Create(session,16,16,1,0,21,0,&texture);
    state.Edit().textures[0]=n::Find(texture);state.Edit().textures[3]=n::Find(texture);
    n::Release(texture);assert(n::HandleCount()==0);
    assert(session->DefaultResourcesLive()); // Current production Reset guard.
    state.ResetPrepared(std::make_shared<p::NativeRenderState>(),false);
    assert(!session->DefaultResourcesLive());
    std::puts("BASELINE production default-resource guard blocks renderer-only texture after all game tokens released; CPU_IMAGE_FIXTURE NO_GPU NO_MAP NO_SWITCH_HARDWARE");
}
