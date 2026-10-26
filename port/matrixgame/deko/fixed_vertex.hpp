// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "material_combiner.hpp"
#include "../native-engine/video_types.hpp"
#include <span>
#include <vector>
namespace srhd_awa::matrixgame::materials {
using Matrix=std::array<float,16>; // Original row-major D3D matrices.
Matrix Identity()noexcept;
Matrix Multiply(const Matrix&,const Matrix&);
struct Vertex {Color position{0,0,0,1},normal{},diffuse{1,1,1,1},specular{};std::array<Color,8> uv{};};
static_assert(sizeof(Vertex)==192);
struct alignas(16) VertexLight {Color direction{},diffuse{},ambient{};};
struct alignas(16) VertexUniforms {
    Matrix clip{},camera{},normal{};
    std::array<Matrix,8> texture{};
    std::array<std::array<std::uint32_t,4>,8> coordinates{}; // TCI, transform flags, input dimension
    Color material_diffuse{},material_ambient{},material_emissive{},global_ambient{};
    std::array<VertexLight,8> lights{};
    std::array<std::uint32_t,4> controls{}; // lighting, normalize, colorvertex, FVF attributes
    std::array<std::uint32_t,4> sources{1,0,0,0}; // diffuse,ambient,emissive
    Color viewport{}; // x,y,width,height
};
static_assert(sizeof(VertexUniforms)==1328);
struct VertexResult {Color clip{},diffuse{};std::array<std::array<float,2>,8> uv{};};
class FixedVertexState {
public:
    FixedVertexState();
    void Layout(std::uint32_t);
    std::uint32_t Layout()const noexcept{return fvf_;}
    std::uint32_t Stride()const noexcept{return stride_;}
    void Transform(std::uint32_t,const Matrix&); // VIEW=2,PROJECTION=3,WORLD=256,TEXTURE=16..23
    Matrix Transform(std::uint32_t)const;
    void Stage(std::uint32_t,std::uint32_t,std::uint32_t);
    std::uint32_t Stage(std::uint32_t,std::uint32_t)const;
    void Material(const platform3g::VideoMaterial&);
    const platform3g::VideoMaterial& Material()const noexcept{return material_;}
    void Light(std::uint32_t,const platform3g::VideoLight&);
    const platform3g::VideoLight& Light(std::uint32_t i)const{return lights_.at(i);}
    bool LightEnabled(std::uint32_t i)const{return enabled_.at(i);}
    void EnableLight(std::uint32_t,bool);
    bool Render(std::uint32_t,std::uint32_t); // false means raster register belongs to another backend
    std::uint32_t Render(std::uint32_t)const;
    void Viewport(const platform3g::VideoViewport&);
    platform3g::VideoViewport Viewport()const noexcept;
    std::vector<Vertex> Decode(std::span<const std::byte>,std::uint32_t stride)const;
    VertexUniforms Prepare()const;
private:
    std::uint32_t fvf_{2},stride_{12};
    std::array<unsigned,8> dimensions_{};
    Matrix world_,view_,projection_;
    VertexUniforms values_;
    platform3g::VideoMaterial material_{};
    std::array<platform3g::VideoLight,8> lights_{};
    std::array<bool,8> enabled_{};
};
VertexResult EvaluateVertex(const VertexUniforms&,const Vertex&);
}
