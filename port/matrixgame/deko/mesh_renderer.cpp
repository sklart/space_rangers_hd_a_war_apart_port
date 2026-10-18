// SPDX-License-Identifier: GPL-2.0-or-later
#include "mesh_renderer.hpp"
#include "texture_upload.hpp"
#include <deko3d.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace srhd_awa::matrixgame {
namespace {
constexpr std::uint32_t command_bytes=65536;
constexpr std::uint32_t max_submissions=256;
constexpr std::uint32_t uniform_stride=DK_UNIFORM_BUF_ALIGNMENT;
constexpr std::uint32_t max_textures=64, sampler_count=4;
constexpr std::uint32_t texture_budget=64*1024*1024;
constexpr auto data_flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
std::uint32_t Align(std::uint64_t value, std::uint32_t alignment=DK_MEMBLOCK_ALIGNMENT) {
  const auto size=(value+alignment-1)&~std::uint64_t(alignment-1);
  if (!value || size>std::numeric_limits<std::uint32_t>::max())
    throw std::length_error("deko memory extent");
  return static_cast<std::uint32_t>(size);
}
void ShaderHeader(std::span<const std::byte> bytes) {
  if (bytes.size()<24) throw std::invalid_argument("short DKSH");
  std::array<std::uint32_t,6> header{}; std::memcpy(header.data(),bytes.data(),24);
  if (header[0]!=0x48534b44 || header[1]<24 || header[1]>header[2] ||
      !header[3] || std::uint64_t(header[2])+header[3]!=bytes.size() ||
      header[4]<header[1] || header[4]>=header[2] || header[5]!=1)
    throw std::invalid_argument("invalid single-program DKSH");
}
struct alignas(16) Uniforms {
  std::array<float,16> clip, normal;
  std::array<float,4> diffuse, light;
};
static_assert(sizeof(Uniforms)==160 && sizeof(Uniforms)<=uniform_stride);
constexpr std::array attributes{
  DkVtxAttribState{0,0,offsetof(MeshVertex,position),DkVtxAttribSize_3x32,DkVtxAttribType_Float,0},
  DkVtxAttribState{0,0,offsetof(MeshVertex,normal),DkVtxAttribSize_3x32,DkVtxAttribType_Float,0},
  DkVtxAttribState{0,0,offsetof(MeshVertex,uv),DkVtxAttribSize_2x32,DkVtxAttribType_Float,0}};
constexpr std::array buffers{DkVtxBufferState{sizeof(MeshVertex),0}};
}
struct DekoMeshRenderer::Impl {
  dk::UniqueDevice device;
  dk::UniqueQueue queue;
  dk::UniqueCmdBuf commands;
  dk::UniqueMemBlock command_mem, vertex_code, fragment_code, color_mem, depth_mem,
      uniforms_mem, vertices_mem, indices_mem, readback_mem,image_descriptors,sampler_descriptors;
  struct Texture { dk::UniqueMemBlock memory; dk::Image image; std::uint32_t bytes{}; };
  std::array<Texture,max_textures> textures;
  std::uint32_t texture_bytes{};
  dk::Shader vertex_shader, fragment_shader;
  dk::Image color, depth;
  std::uint32_t width{},height{},row_bytes{},vertex_bytes{},index_count{};
  std::vector<std::uint16_t> index_shadow;
  dk::MemBlock Allocate(std::uint64_t size,std::uint32_t flags) {
    auto memory=dk::MemBlockMaker{device,Align(size)}.setFlags(flags).create();
    if (!memory) throw std::bad_alloc();
    return memory;
  }
  void LoadShader(std::span<const std::byte> bytes,dk::UniqueMemBlock& memory,dk::Shader& shader) {
    ShaderHeader(bytes);
    memory=Allocate(bytes.size()+DK_SHADER_CODE_UNUSABLE_SIZE,data_flags|DkMemBlockFlags_Code);
    std::memcpy(memory.getCpuAddr(),bytes.data(),bytes.size());
    dk::ShaderMaker{memory,0}.initialize(shader);
  }
  void UploadTexture(std::uint32_t slot,std::uint32_t w,std::uint32_t h,std::uint32_t pitch,
      std::span<const std::byte> rgba) {
    const auto plan=ValidateRgba8Upload(w,h,pitch,rgba.size());
    dk::ImageLayout layout;
    dk::ImageLayoutMaker{device}.setFlags(0).setFormat(DkImageFormat_RGBA8_Unorm).setDimensions(w,h).initialize(layout);
    if(layout.getAlignment()>DK_MEMBLOCK_ALIGNMENT) throw std::runtime_error("deko texture alignment");
    const auto image_bytes=Align(layout.getSize()),staging_bytes=Align(plan.staging_bytes);
    // Account old+new+staging, including replacement's temporary peak. Failure
    // preserves the old slot instead of dropping it to make room implicitly.
    if(std::uint64_t(texture_bytes)+image_bytes+staging_bytes>texture_budget)
      throw std::length_error("deko texture upload budget");
    dk::UniqueMemBlock storage{Allocate(image_bytes,DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image)};
    dk::UniqueMemBlock staging{Allocate(staging_bytes,data_flags)};
    PackRgba8Rows(plan,rgba,{static_cast<std::byte*>(staging.getCpuAddr()),plan.staging_bytes});
    dk::Image image; image.initialize(layout,storage,0);
    dk::ImageView view{image}; dk::ImageDescriptor descriptor; descriptor.initialize(view);
    queue.waitIdle(); commands.clear(); commands.addMemory(command_mem,0,command_bytes);
    commands.copyBufferToImage({staging.getGpuAddr(),plan.staging_pitch/4,h},view,{0,0,0,w,h,1});
    commands.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);
    queue.submitCommands(commands.finishList()); queue.flush(); queue.waitIdle(); commands.clear();
    std::memcpy(static_cast<std::byte*>(image_descriptors.getCpuAddr())+slot*sizeof(DkImageDescriptor),
        &descriptor,sizeof(descriptor));
    auto& target=textures[slot];
    texture_bytes=texture_bytes-target.bytes+image_bytes;
    target.memory=std::move(storage); target.image=image; target.bytes=image_bytes;
  }
  Impl(std::uint32_t w,std::uint32_t h,std::span<const std::byte> vs,std::span<const std::byte> fs)
      :width(w),height(h) {
    if (!w || !h || w>2048 || h>2048) throw std::invalid_argument("deko target dimensions");
    ShaderHeader(vs); ShaderHeader(fs);
    device=dk::DeviceMaker{}.setFlags(DkDeviceFlags_DepthZeroToOne|DkDeviceFlags_OriginUpperLeft).create();
    if (!device) throw std::runtime_error("deko device creation failed");
    queue=dk::QueueMaker{device}.setFlags(DkQueueFlags_Graphics).create();
    if (!queue) throw std::runtime_error("deko graphics queue creation failed");
    commands=dk::CmdBufMaker{device}.create();
    if (!commands) throw std::runtime_error("deko command buffer creation failed");
    command_mem=Allocate(command_bytes,data_flags);
    LoadShader(vs,vertex_code,vertex_shader); LoadShader(fs,fragment_code,fragment_shader);
    uniforms_mem=Allocate(max_submissions*uniform_stride,data_flags);
    image_descriptors=Allocate(max_textures*sizeof(DkImageDescriptor),data_flags);
    sampler_descriptors=Allocate(sampler_count*sizeof(DkSamplerDescriptor),data_flags);
    std::memset(image_descriptors.getCpuAddr(),0,max_textures*sizeof(DkImageDescriptor));
    for(std::uint32_t i=0;i<sampler_count;++i) {
      dk::Sampler sampler;
      const auto filter=(i&1) ? DkFilter_Linear : DkFilter_Nearest;
      const auto wrap=(i&2) ? DkWrapMode_ClampToEdge : DkWrapMode_Repeat;
      sampler.setFilter(filter,filter).setWrapMode(wrap,wrap,wrap);
      dk::SamplerDescriptor descriptor; descriptor.initialize(sampler);
      std::memcpy(static_cast<std::byte*>(sampler_descriptors.getCpuAddr())+i*sizeof(descriptor),&descriptor,sizeof(descriptor));
    }
    dk::ImageLayout color_layout,depth_layout;
    dk::ImageLayoutMaker{device}.setFlags(DkImageFlags_UsageRender)
      .setFormat(DkImageFormat_RGBA8_Unorm).setDimensions(w,h).initialize(color_layout);
    dk::ImageLayoutMaker{device}.setFlags(DkImageFlags_UsageRender)
      .setFormat(DkImageFormat_Z24S8).setDimensions(w,h).initialize(depth_layout);
    if(color_layout.getAlignment()>DK_MEMBLOCK_ALIGNMENT || depth_layout.getAlignment()>DK_MEMBLOCK_ALIGNMENT)
      throw std::runtime_error("deko image alignment exceeds prototype allocator");
    color_mem=Allocate(color_layout.getSize(),DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image);
    depth_mem=Allocate(depth_layout.getSize(),DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image);
    color.initialize(color_layout,color_mem,0); depth.initialize(depth_layout,depth_mem,0);
    row_bytes=Align(std::uint64_t(w)*4,DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
    readback_mem=Allocate(std::uint64_t(row_bytes)*h,DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuUncached);
    const std::array<std::byte,4> white{std::byte{255},std::byte{255},std::byte{255},std::byte{255}};
    UploadTexture(0,1,1,4,white);
  }
  ~Impl() {
    if(queue) queue.waitIdle();
    // Command backing memory must outlive the command buffer object itself.
    commands=nullptr; queue=nullptr;
  }
};
DekoMeshRenderer::DekoMeshRenderer(std::uint32_t w,std::uint32_t h,
    std::span<const std::byte> vs,std::span<const std::byte> fs)
    :impl_(std::make_unique<Impl>(w,h,vs,fs)) {}
DekoMeshRenderer::~DekoMeshRenderer()=default;
void DekoMeshRenderer::SetMesh(std::span<const MeshVertex> vertices,std::span<const std::uint16_t> indices) {
  if(vertices.empty() || vertices.size()>65536 || indices.empty() || indices.size()>1048576)
    throw std::invalid_argument("deko triangle mesh extent");
  std::vector<std::uint16_t> new_shadow(indices.begin(),indices.end());
  for(const auto& vertex:vertices) {
    for(float value:vertex.position) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite vertex");
    for(float value:vertex.normal) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite normal");
    for(float value:vertex.uv) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite uv");
  }
  dk::UniqueMemBlock new_vertices{impl_->Allocate(vertices.size_bytes(),data_flags)};
  dk::UniqueMemBlock new_indices{impl_->Allocate(indices.size_bytes(),data_flags)};
  std::memcpy(new_vertices.getCpuAddr(),vertices.data(),vertices.size_bytes());
  std::memcpy(new_indices.getCpuAddr(),indices.data(),indices.size_bytes());
  impl_->queue.waitIdle(); impl_->commands.clear();
  impl_->vertices_mem=std::move(new_vertices); impl_->indices_mem=std::move(new_indices);
  impl_->vertex_bytes=vertices.size_bytes(); impl_->index_count=indices.size();
  impl_->index_shadow=std::move(new_shadow);
}
void DekoMeshRenderer::SetTextureRGBA8(std::uint32_t slot,std::uint32_t w,std::uint32_t h,
    std::uint32_t pitch,std::span<const std::byte> rgba) {
  if(!slot || slot>=max_textures) throw std::out_of_range("deko user texture slot");
  impl_->UploadTexture(slot,w,h,pitch,rgba);
}
void DekoMeshRenderer::ClearTexture(std::uint32_t slot) {
  if(!slot || slot>=max_textures) throw std::out_of_range("deko user texture slot");
  auto& v=*impl_; v.queue.waitIdle(); v.commands.clear();
  auto& target=v.textures[slot]; v.texture_bytes-=target.bytes;
  target.memory=nullptr; target.bytes=0;
  std::memset(static_cast<std::byte*>(v.image_descriptors.getCpuAddr())+slot*sizeof(DkImageDescriptor),0,sizeof(DkImageDescriptor));
}
void DekoMeshRenderer::Draw(const MeshDraw& draw,std::span<std::byte> output) {
  if(!impl_->index_count) throw std::logic_error("deko mesh not uploaded");
  const MeshSubmission submission{{0,0,0,impl_->index_count,0,static_cast<std::uint32_t>(impl_->vertex_bytes/sizeof(MeshVertex))},draw};
  DrawFrame(std::span(&submission,1),output);
}
void DekoMeshRenderer::DrawFrame(std::span<const MeshSubmission> submissions,std::span<std::byte> output) {
  auto& v=*impl_;
  if(submissions.size()>max_submissions) throw std::length_error("deko batch draw count");
  if(output.size()<std::uint64_t(v.width)*v.height*4) throw std::length_error("deko RGBA output");
  // Validate the whole batch before replacing commands or publishing uniforms.
  for(const auto& submission:submissions) {
    ValidateMeshRange(submission.range,v.index_shadow,v.vertex_bytes/sizeof(MeshVertex));
    if(submission.texture_slot>=max_textures || !v.textures[submission.texture_slot].memory ||
        static_cast<std::uint32_t>(submission.sampling)>=sampler_count)
      throw std::out_of_range("deko texture/sampler not available");
    const auto& draw=submission.draw;
    for(const auto& values:{std::span<const float>(draw.clip_from_model),std::span<const float>(draw.normal_from_model),
         std::span<const float>(draw.diffuse),std::span<const float>(draw.light)})
      for(float value:values) if(!std::isfinite(value)) throw std::invalid_argument("nonfinite draw uniforms");
  }
  for(std::size_t i=0;i<submissions.size();++i) {
    const auto& draw=submissions[i].draw;
    const Uniforms uniforms{draw.clip_from_model,draw.normal_from_model,draw.diffuse,draw.light};
    std::memcpy(static_cast<std::byte*>(v.uniforms_mem.getCpuAddr())+i*uniform_stride,&uniforms,sizeof(uniforms));
  }
  v.commands.clear(); v.commands.addMemory(v.command_mem,0,command_bytes);
  dk::ImageView color_target{v.color},depth_target{v.depth};
  v.commands.bindRenderTargets(&color_target,&depth_target);
  v.commands.setViewports(0,{{0,0,float(v.width),float(v.height),0,1}});
  v.commands.setScissors(0,{{0,0,v.width,v.height}});
  v.commands.clearColor(0,DkColorMask_RGBA,0.f,0.f,0.f,1.f);
  v.commands.clearDepthStencil(true,1.f,0xff,0);
  dk::RasterizerState rasterizer; rasterizer.setCullMode(DkFace_None);
  dk::ColorState color_state; dk::ColorWriteState color_write;
  dk::DepthStencilState depth_state;
  depth_state.setDepthTestEnable(true).setDepthWriteEnable(true).setDepthCompareOp(DkCompareOp_Lequal);
  v.commands.bindRasterizerState(rasterizer); v.commands.bindColorState(color_state);
  v.commands.bindColorWriteState(color_write); v.commands.bindDepthStencilState(depth_state);
  v.commands.bindShaders(DkStageFlag_GraphicsMask,{&v.vertex_shader,&v.fragment_shader});
  v.commands.barrier(DkBarrier_None,DkInvalidateFlags_Shader|DkInvalidateFlags_L2Cache|
      DkInvalidateFlags_Image|DkInvalidateFlags_Descriptors);
  v.commands.bindImageDescriptorSet(v.image_descriptors.getGpuAddr(),max_textures);
  v.commands.bindSamplerDescriptorSet(v.sampler_descriptors.getGpuAddr(),sampler_count);
  if(v.index_count) {
    v.commands.bindVtxAttribState(attributes); v.commands.bindVtxBufferState(buffers);
    v.commands.bindVtxBuffer(0,v.vertices_mem.getGpuAddr(),v.vertex_bytes);
    v.commands.bindIdxBuffer(DkIdxFormat_Uint16,v.indices_mem.getGpuAddr());
  }
  for(std::size_t i=0;i<submissions.size();++i) {
    const auto& range=submissions[i].range;
    if(!range.index_count) continue;
    const auto& submission=submissions[i];
    const DkResHandle texture=dkMakeTextureHandle(submission.texture_slot,static_cast<std::uint32_t>(submission.sampling));
    v.commands.bindTextures(DkStage_Fragment,0,texture);
    v.commands.bindUniformBuffer(DkStage_Vertex,0,v.uniforms_mem.getGpuAddr()+i*uniform_stride,sizeof(Uniforms));
    v.commands.drawIndexed(DkPrimitive_Triangles,range.index_count,1,range.first_index,range.base_vertex,0);
  }
  v.commands.barrier(DkBarrier_Full,DkInvalidateFlags_L2Cache);
  v.commands.copyImageToBuffer(color_target,{0,0,0,v.width,v.height,1},
      {v.readback_mem.getGpuAddr(),v.row_bytes/4,v.height});
  v.queue.submitCommands(v.commands.finishList()); v.queue.flush(); v.queue.waitIdle();
  auto* pixels=static_cast<const std::byte*>(v.readback_mem.getCpuAddr());
  for(std::uint32_t y=0;y<v.height;++y)
    std::memcpy(output.data()+std::size_t(y)*v.width*4,pixels+std::size_t(y)*v.row_bytes,v.width*4);
}
} // namespace srhd_awa::matrixgame
