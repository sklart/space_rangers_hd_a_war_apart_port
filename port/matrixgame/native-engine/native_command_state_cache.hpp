// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include "../deko/stencil_state.hpp"
namespace srhd_awa::matrixgame::platform3g {
// Only state emitted by the original draw path belongs here. Invalidate when
// command recording is restarted or another renderer changes these bindings.
class NativeCommandStateCache {
public:
    struct DepthKey {
        bool depth_test{},depth_write{};std::uint32_t depth_compare{};MeshStencilState stencil;
        bool operator==(const DepthKey&)const=default;
    };
private:
    bool vertex_layout_{},multisample_{},material_bindings_{},line_state_{};
    std::optional<std::array<float,4>> viewport_;
    std::optional<std::array<unsigned,4>> scissor_;
    std::optional<std::array<std::uint32_t,2>> raster_;
    std::optional<std::array<std::uint32_t,11>> color_;
    std::optional<DepthKey> depth_;
public:
    void Invalidate() noexcept {vertex_layout_=false;multisample_=false;material_bindings_=false;line_state_=false;viewport_.reset();scissor_.reset();raster_.reset();color_.reset();depth_.reset();}
    bool NeedsVertexLayout() const noexcept {return !vertex_layout_;}
    void VertexLayoutBound() noexcept {vertex_layout_=true;}
    bool NeedsMultisample() const noexcept {return !multisample_;}
    void MultisampleBound() noexcept {multisample_=true;}
    bool NeedsMaterialBindings() const noexcept {return !material_bindings_;}
    void MaterialBindingsBound() noexcept {material_bindings_=true;}
    bool NeedsLineState()const noexcept{return !line_state_;}
    void LineStateBound()noexcept{line_state_=true;}
    bool NeedsViewport(const std::array<float,4>& v) const noexcept {return !viewport_ || *viewport_!=v;}
    void ViewportBound(const std::array<float,4>& v) noexcept {viewport_=v;}
    bool NeedsScissor(const std::array<unsigned,4>& v) const noexcept {return !scissor_ || *scissor_!=v;}
    void ScissorBound(const std::array<unsigned,4>& v) noexcept {scissor_=v;}
    void InvalidateScissor() noexcept {scissor_.reset();}
    bool NeedsRaster(const std::array<std::uint32_t,2>& v)const noexcept{return !raster_ || *raster_!=v;}
    void RasterBound(const std::array<std::uint32_t,2>& v)noexcept{raster_=v;}
    bool NeedsColor(const std::array<std::uint32_t,11>& v)const noexcept{return !color_ || *color_!=v;}
    void ColorBound(const std::array<std::uint32_t,11>& v)noexcept{color_=v;}
    bool NeedsDepth(const DepthKey& v)const noexcept{return !depth_ || *depth_!=v;}
    void DepthBound(const DepthKey& v)noexcept{depth_=v;}
};
}
