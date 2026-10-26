// SPDX-License-Identifier: GPL-2.0-or-later
#include "material_program.hpp"
#include <cstring>
#include <limits>
namespace srhd_awa::matrixgame::materials {
namespace {
constexpr std::uint32_t FogOffset=(sizeof(Uniforms)+DK_UNIFORM_BUF_ALIGNMENT-1)&~(DK_UNIFORM_BUF_ALIGNMENT-1);
static_assert(FogOffset==512);
static_assert(FogOffset+sizeof(PixelFogUniforms)<=DK_MEMBLOCK_ALIGNMENT); // Same allocation/budget as before.
constexpr std::uint32_t RasterOffset=768;
static_assert(RasterOffset+16<=DK_MEMBLOCK_ALIGNMENT && FogOffset+sizeof(PixelFogUniforms)<=RasterOffset);
std::uint32_t Size(std::size_t bytes){
    if(!bytes || bytes>std::numeric_limits<std::uint32_t>::max()-(DK_MEMBLOCK_ALIGNMENT-1))throw std::length_error("material allocation overflow");
    return (std::uint32_t(bytes)+DK_MEMBLOCK_ALIGNMENT-1)&~(DK_MEMBLOCK_ALIGNMENT-1);
}
}
DekoCombinerProgram::DekoCombinerProgram(dk::Device device,std::span<const std::byte> bytes,std::span<const std::byte> vertex):device_(device){
    if(!device)throw std::invalid_argument("material program requires a real device");
    if(bytes.size()<24)throw std::invalid_argument("short material DKSH");
    std::array<std::uint32_t,6> h;std::memcpy(h.data(),bytes.data(),24);
    if(h[0]!=0x48534b44 || h[1]<24 || h[1]>h[2] || !h[3] || std::uint64_t(h[2])+h[3]!=bytes.size() || h[4]<h[1] || h[4]>=h[2] || h[5]!=1)
        throw std::invalid_argument("invalid material DKSH extent");
    if(bytes.size()>std::numeric_limits<std::uint32_t>::max()-DK_SHADER_CODE_UNUSABLE_SIZE)throw std::length_error("material code overflow");
    code_=dk::MemBlockMaker{device,Size(bytes.size()+DK_SHADER_CODE_UNUSABLE_SIZE)}
        .setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code).create();
    if(!code_)throw std::bad_alloc();
    std::memcpy(code_.getCpuAddr(),bytes.data(),bytes.size());
    dk::ShaderMaker{code_,0}.initialize(shader_);
    if(shader_.getStage()!=DkStage_Fragment)throw std::invalid_argument("material code is not fragment stage");
    if(!vertex.empty()){
        // Same extent validation as fragment; SDK initialization checks stage.
        if(vertex.size()<24)throw std::invalid_argument("short vertex DKSH");
        std::memcpy(h.data(),vertex.data(),24);
        if(h[0]!=0x48534b44 || h[1]<24 || h[1]>h[2] || !h[3] || std::uint64_t(h[2])+h[3]!=vertex.size() || h[4]<h[1] || h[4]>=h[2] || h[5]!=1)
            throw std::invalid_argument("invalid vertex DKSH extent");
        if(vertex.size()>std::numeric_limits<std::uint32_t>::max()-DK_SHADER_CODE_UNUSABLE_SIZE)throw std::length_error("vertex code overflow");
        vertex_code_=dk::MemBlockMaker{device,Size(vertex.size()+DK_SHADER_CODE_UNUSABLE_SIZE)}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code).create();
        if(!vertex_code_)throw std::bad_alloc();std::memcpy(vertex_code_.getCpuAddr(),vertex.data(),vertex.size());
        dk::ShaderMaker{vertex_code_,0}.initialize(vertex_shader_);
        if(vertex_shader_.getStage()!=DkStage_Vertex)throw std::invalid_argument("material vertex code has wrong stage");
    }
}
bool DekoCombinerProgram::VertexReady()const noexcept{return vertex_code_ && dkShaderIsValid(&vertex_shader_) && dkShaderIsValid(&shader_);}
DekoCombinerProgram::Packet DekoCombinerProgram::Prepare(const Combiner& state,const std::array<DkResHandle,8>& textures,const VertexUniforms* vertex,bool alpha_target,const PixelFogUniforms* fog,std::uint32_t shade)const{
    if(shade!=1 && shade!=2)throw Incomplete("native material shading mode outside FLAT/GOURAUD");
    auto value=state.Prepare();AlphaTarget(value,alpha_target); // Reject before allocation or publication.
    Packet out{dk::MemBlockMaker{device_,Size(RasterOffset+16)}
        .setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create(),{},textures};
    if(!out.uniforms)throw std::bad_alloc();
    std::memcpy(out.uniforms.getCpuAddr(),&value,sizeof(value));
    const PixelFogUniforms disabled;std::memcpy(static_cast<std::byte*>(out.uniforms.getCpuAddr())+FogOffset,fog?fog:&disabled,sizeof(disabled));
    const std::array<std::uint32_t,4> controls{shade,0,0,0};std::memcpy(static_cast<std::byte*>(out.uniforms.getCpuAddr())+RasterOffset,controls.data(),sizeof(controls));
    if(vertex){if(!VertexReady())throw Incomplete("material vertex program unavailable");out.vertex_uniforms=dk::MemBlockMaker{device_,Size(sizeof(*vertex))}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();if(!out.vertex_uniforms)throw std::bad_alloc();std::memcpy(out.vertex_uniforms.getCpuAddr(),vertex,sizeof(*vertex));}
    return out;
}
void DekoCombinerProgram::Bind(dk::CmdBuf commands,const Packet& packet)const{
    if(!commands || !packet.uniforms)throw std::invalid_argument("material command/packet unavailable");
    commands.bindShaders(DkStageFlag_Fragment,{&shader_});
    if(packet.vertex_uniforms){commands.bindShaders(DkStageFlag_Vertex,{&vertex_shader_});commands.bindUniformBuffer(DkStage_Vertex,0,dkMemBlockGetGpuAddr(packet.vertex_uniforms),sizeof(VertexUniforms));}
    commands.bindUniformBuffer(DkStage_Fragment,0,dkMemBlockGetGpuAddr(packet.uniforms),sizeof(Uniforms));
    commands.bindUniformBuffer(DkStage_Fragment,1,dkMemBlockGetGpuAddr(packet.uniforms)+FogOffset,sizeof(PixelFogUniforms));
    commands.bindUniformBuffer(DkStage_Fragment,2,dkMemBlockGetGpuAddr(packet.uniforms)+RasterOffset,16);
    commands.bindTextures(DkStage_Fragment,0,packet.textures);
}
}
