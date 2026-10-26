// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "native_texture.hpp"
#include "buffer_resources.hpp"
#include "../deko/fixed_vertex.hpp"
#include "../deko/pixel_fog.hpp"
#include "../deko/stencil_state.hpp"
#include <map>
namespace srhd_awa::matrixgame::platform3g {
// All mutable state consumed by DekoDevice::Record. Shader programs and owned
// presentation targets have a separate restore lease; acquire/submit is not snapshot state.
struct NativeRenderState {
    materials::Combiner combiner;
    materials::FixedVertexState vertex;
    std::array<std::shared_ptr<native_texture::Texture>,8> textures;
    std::array<MeshSamplerState,8> samplers;
    MeshRasterState raster;
    std::uint32_t cull{3},shade{2};
    std::map<std::uint32_t,std::uint32_t> registers;
    // BORDERCOLOR, MAXMIPLEVEL, SRGBTEXTURE, ELEMENTINDEX, DMAPOFFSET.
    std::array<std::array<std::uint32_t,5>,8> sampler_extra{};
    NativeRenderState();
    void Render(std::uint32_t,std::uint32_t);
    std::uint32_t Render(std::uint32_t)const;
    void Sampler(std::uint32_t,std::uint32_t,std::uint32_t);
    std::uint32_t Sampler(std::uint32_t,std::uint32_t)const;
    void ValidateDraw(bool points=false,bool lines=false)const;
    void ValidateClear()const;
    materials::PixelFogUniforms PixelFog()const;
    MeshStencilState Stencil()const;
};
class NativeStateStore {
public:
    NativeStateStore();
    ~NativeStateStore();
    NativeStateStore(const NativeStateStore&)=delete;
    const NativeRenderState& Read()const;
    NativeRenderState& Edit();
    std::unique_ptr<DeviceStateLease> Capture();
    void ResetPrepared(std::shared_ptr<NativeRenderState>,bool clear_bindings=true)noexcept;
    std::size_t Outstanding()const noexcept;
    void AddDeviceTextureReferences(native_texture::DeviceReferences&)const noexcept;
    void Frame(bool)noexcept;
    void Close()noexcept;
private:
    struct Control;
    struct Lease;
    std::shared_ptr<Control> control_;
};
}
