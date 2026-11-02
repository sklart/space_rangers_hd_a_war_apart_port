// SPDX-License-Identifier: GPL-2.0-or-later
#include "gamma_program.hpp"
#include <cstring>
#include <limits>
namespace srhd_awa::matrixgame {
namespace {
constexpr unsigned ImageOffset=4096,SamplerOffset=8192,PacketBytes=12288;
static_assert(sizeof(platform3g::GammaUniforms)<=ImageOffset);
static_assert(sizeof(DkImageDescriptor)<=SamplerOffset-ImageOffset && sizeof(DkSamplerDescriptor)<=PacketBytes-SamplerOffset);
static_assert(ImageOffset%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0 && SamplerOffset%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
unsigned Align(std::size_t bytes){
    if(bytes>UINT32_MAX-(DK_MEMBLOCK_ALIGNMENT-1))throw std::length_error("gamma GPU allocation overflow");
    return (unsigned(bytes)+DK_MEMBLOCK_ALIGNMENT-1)&~(DK_MEMBLOCK_ALIGNMENT-1);
}
}
DekoGammaProgram::DekoGammaProgram(dk::Device device,std::span<const std::byte> vertex,std::span<const std::byte> fragment):device_(device){
    if(!device)throw std::invalid_argument("gamma requires live device");
    const std::span<const std::byte> sources[]{vertex,fragment};
    const DkStage stages[]{DkStage_Vertex,DkStage_Fragment};
    for(unsigned i=0;i<2;++i){
        const auto bytes=sources[i];std::array<unsigned,6> h{};
        if(bytes.size()<24)throw std::invalid_argument("short gamma DKSH");std::memcpy(h.data(),bytes.data(),24);
        if(h[0]!=0x48534b44 || h[1]<24 || h[1]>h[2] || !h[3] || std::uint64_t(h[2])+h[3]!=bytes.size() || h[4]<h[1] || h[4]>=h[2] || h[5]!=1)
            throw std::invalid_argument("invalid gamma DKSH extent");
        code_[i]=dk::MemBlockMaker{device_,Align(bytes.size()+DK_SHADER_CODE_UNUSABLE_SIZE)}
            .setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached|DkMemBlockFlags_Code).create();
        if(!code_[i])throw std::bad_alloc();std::memcpy(code_[i].getCpuAddr(),bytes.data(),bytes.size());
        dk::ShaderMaker{code_[i],0}.initialize(shaders_[i]);
        if(!dkShaderIsValid(&shaders_[i]) || shaders_[i].getStage()!=stages[i])throw std::invalid_argument("invalid gamma shader stage");
    }
}
DekoGammaProgram::Packet DekoGammaProgram::Prepare(const dk::Image& source,const platform3g::GammaRamp& ramp)const {
    Packet packet{dk::MemBlockMaker{device_,PacketBytes}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create()};
    if(!packet.memory)throw std::bad_alloc();auto* address=static_cast<std::byte*>(packet.memory.getCpuAddr());
    const auto uniforms=platform3g::PrepareGamma(ramp);std::memcpy(address,&uniforms,sizeof(uniforms));
    dk::ImageView view{source};dk::ImageDescriptor image;image.initialize(view);
    dk::Sampler sampler;sampler.setFilter(DkFilter_Nearest,DkFilter_Nearest,DkMipFilter_None).setWrapMode(DkWrapMode_ClampToEdge,DkWrapMode_ClampToEdge,DkWrapMode_ClampToEdge);
    dk::SamplerDescriptor sampled;sampled.initialize(sampler);
    std::memcpy(address+ImageOffset,&image,sizeof(image));std::memcpy(address+SamplerOffset,&sampled,sizeof(sampled));return packet;
}
void DekoGammaProgram::Record(dk::CmdBuf commands,const Packet& packet,const dk::Image& output,unsigned width,unsigned height)const {
    if(!commands || !packet.memory || !width || !height)throw std::invalid_argument("gamma submission extent/packet");
    dk::ImageView target{output};commands.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);
    commands.bindRenderTargets(&target,nullptr);
    commands.setViewports(0,{{0,0,float(width),float(height),0,1}});commands.setScissors(0,{{0,0,width,height}});
    dk::RasterizerState raster;raster.setCullMode(DkFace_None).setPolygonMode(DkPolygonMode_Fill);
    commands.bindRasterizerState(raster);commands.bindMultisampleState(dk::MultisampleState{});
    dk::ColorState color;color.setBlendEnable(0,false).setAlphaCompareOp(DkCompareOp_Always);
    dk::ColorWriteState write;write.setMask(0,DkColorMask_RGBA);
    dk::DepthStencilState depth;depth.setDepthTestEnable(false).setDepthWriteEnable(false).setStencilTestEnable(false);
    commands.bindColorState(color);commands.bindColorWriteState(write);commands.bindDepthStencilState(depth);
    commands.bindVtxAttribState({});commands.bindVtxBufferState({});
    commands.bindShaders(DkStageFlag_GraphicsMask,{&shaders_[0],&shaders_[1]});
    const auto gpu=dkMemBlockGetGpuAddr(packet.memory);commands.bindUniformBuffer(DkStage_Fragment,0,gpu,sizeof(platform3g::GammaUniforms));
    commands.bindImageDescriptorSet(gpu+ImageOffset,1);commands.bindSamplerDescriptorSet(gpu+SamplerOffset,1);
    commands.bindTextures(DkStage_Fragment,0,{dkMakeTextureHandle(0,0)});
    commands.barrier(DkBarrier_None,DkInvalidateFlags_Shader|DkInvalidateFlags_Descriptors|DkInvalidateFlags_L2Cache);
    commands.draw(DkPrimitive_Triangles,3,1,0,0);
}
}
