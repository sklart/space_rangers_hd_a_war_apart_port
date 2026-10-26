// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_store.hpp"
#include <cstring>
#include <limits>
namespace srhd_awa::matrixgame::native_texture {
namespace {
std::uint32_t Align(std::uint64_t n){if(!n || n>std::numeric_limits<std::uint32_t>::max()-(DK_MEMBLOCK_ALIGNMENT-1))throw std::length_error("texture memory extent");return (n+DK_MEMBLOCK_ALIGNMENT-1)&~std::uint64_t(DK_MEMBLOCK_ALIGNMENT-1);}
DkImageFormat Format(std::uint32_t f){switch(f){
case 75:return DkImageFormat_Z24S8;case 77:return DkImageFormat_Z24X8;case 80:return DkImageFormat_Z16;
case 21:return DkImageFormat_BGRA8_Unorm;case 22:return DkImageFormat_BGRX8_Unorm;
case 23:return DkImageFormat_BGR565_Unorm;case 25:return DkImageFormat_BGR5A1_Unorm;
case 24:return DkImageFormat_BGR5_Unorm;
case 26:return DkImageFormat_RGBA4_Unorm; // Swizzled below: memory is BGRA4444.
case 28:case 50:return DkImageFormat_R8_Unorm;case 51:return DkImageFormat_RG8_Unorm;
case 0x31545844:return DkImageFormat_RGBA_BC1;case 0x33545844:return DkImageFormat_RGBA_BC2;case 0x35545844:return DkImageFormat_RGBA_BC3;
default:throw platform3g::Unsupported("deko texture image format");}}
}
struct DekoTextureStore::Arena {
    dk::UniqueMemBlock memory;
    Residency residency;
    Arena(DkDevice device,std::uint32_t bytes):residency(bytes){
        memory=dk::MemBlockMaker{device,bytes}.setFlags(DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image).create();
        if(!memory)throw std::bad_alloc();
    }
};
DekoTextureStore::ResidentImage::~ResidentImage(){if(arena && range.size)arena->residency.Release(range);}
DekoTextureStore::DekoTextureStore(DkDevice device,DkQueue queue,std::uint32_t bytes):device_(device),queue_(queue){
    if(!bytes || bytes%DK_MEMBLOCK_ALIGNMENT)throw std::invalid_argument("texture arena backing alignment");
    arena_=std::make_shared<Arena>(device,bytes);
    commands_=dk::CmdBufMaker{device}.create();if(!commands_)throw std::bad_alloc();
    command_memory_=dk::MemBlockMaker{device,DK_MEMBLOCK_ALIGNMENT}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();
    if(!command_memory_)throw std::bad_alloc();
}
std::uint64_t DekoTextureStore::Available()const noexcept{return arena_->residency.Largest();}
std::uint64_t DekoTextureStore::Free()const noexcept{return arena_->residency.Free();}
std::uint64_t DekoTextureStore::Capacity()const noexcept{return arena_->residency.Capacity();}
std::shared_ptr<Image> DekoTextureStore::Upload(const Texture& texture){
    ValidateUpload(texture);
    dk::ImageLayout layout;const auto& top=texture.levels.front();
    dk::ImageLayoutMaker{device_}.setFlags(texture.usage&1u?DkImageFlags_UsageRender:0).setFormat(Format(texture.format)).setDimensions(top.width,top.height)
        .setMipLevels(texture.levels.size()).initialize(layout);
    if(!layout.getSize() || layout.getSize()>std::numeric_limits<std::uint32_t>::max())throw std::length_error("native texture GPU extent");
    auto image=std::make_shared<ResidentImage>();image->arena=arena_;
    image->range=arena_->residency.Allocate(std::uint32_t(layout.getSize()),layout.getAlignment());
    image->image.initialize(layout,arena_->memory,image->range.offset);
    dk::ImageView sampled{image->image};
    if(texture.format==26)sampled.setSwizzle(DkImageSwizzle_Blue,DkImageSwizzle_Green,DkImageSwizzle_Red,DkImageSwizzle_Alpha);
    if(texture.format==28)sampled.setSwizzle(DkImageSwizzle_One,DkImageSwizzle_One,DkImageSwizzle_One,DkImageSwizzle_Red);
    if(texture.format==50)sampled.setSwizzle(DkImageSwizzle_Red,DkImageSwizzle_Red,DkImageSwizzle_Red,DkImageSwizzle_One);
    if(texture.format==51)sampled.setSwizzle(DkImageSwizzle_Red,DkImageSwizzle_Red,DkImageSwizzle_Red,DkImageSwizzle_Green);
    image->descriptor.initialize(sampled);
    // Independent transfer command buffer. Previously recorded draw packets
    // keep their old image version; writes never mutate an in-flight image.
    for(unsigned i=0;i<texture.levels.size();++i){const auto& level=texture.levels[i];
        auto staging=dk::MemBlockMaker{device_,Align(level.bytes.size())}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();
        if(!staging)throw std::bad_alloc();std::memcpy(staging.getCpuAddr(),level.bytes.data(),level.bytes.size());
        commands_.clear();commands_.addMemory(command_memory_,0,DK_MEMBLOCK_ALIGNMENT);
        dk::ImageView destination{image->image};destination.setMipLevels(i,1);
        DkCopyBuf source{staging.getGpuAddr(),0,0};DkImageRect rect{0,0,0,level.width,level.height,1};
        commands_.barrier(DkBarrier_None,DkInvalidateFlags_L2Cache);
        commands_.copyBufferToImage(source,destination,rect);
        commands_.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);
        dkQueueSubmitCommands(queue_,commands_.finishList());dkQueueFlush(queue_);dkQueueWaitIdle(queue_);
        if(dkQueueIsInErrorState(queue_))throw std::runtime_error("native texture upload queue error");
        commands_.clear();
    }
    return image;
}
std::shared_ptr<DekoTextureStore::ResidentImage> DekoTextureStore::AllocateTarget(std::uint32_t w,std::uint32_t h,std::uint32_t format){
    if(!w || !h || w>8192 || h>8192 || (format!=21 && format!=22 && format!=23 && format!=28 && format!=75 && format!=77 && format!=80))throw platform3g::Unsupported("deko offscreen target extent/format");
    dk::ImageLayout layout;dk::ImageLayoutMaker{device_}.setFlags(DkImageFlags_UsageRender).setFormat(Format(format)).setDimensions(w,h).initialize(layout);
    if(!layout.getSize() || layout.getSize()>UINT32_MAX)throw std::length_error("deko offscreen GPU extent");
    auto image=std::make_shared<ResidentImage>();image->arena=arena_;image->range=arena_->residency.Allocate(std::uint32_t(layout.getSize()),layout.getAlignment());
    image->image.initialize(layout,arena_->memory,image->range.offset);dk::ImageView sampled{image->image};image->descriptor.initialize(sampled);return image;
}
std::vector<std::byte> DekoTextureStore::ReadImage(const dk::Image& image,std::uint32_t w,std::uint32_t h,std::uint32_t pixel_bytes,std::uint32_t level){
    const std::uint64_t bytes=std::uint64_t(w)*h*pixel_bytes;if(!w || !h || w>8192 || h>8192 || (pixel_bytes!=1 && pixel_bytes!=2 && pixel_bytes!=4) || bytes>64*1024*1024)throw std::invalid_argument("offscreen readback extent");
    if(dkQueueIsInErrorState(queue_))throw std::runtime_error("offscreen readback queue lost");
    auto staging=dk::MemBlockMaker{device_,Align(bytes)}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();if(!staging)throw std::bad_alloc();
    std::vector<std::byte> result(bytes);commands_.clear();commands_.addMemory(command_memory_,0,DK_MEMBLOCK_ALIGNMENT);
    dk::ImageView source{image};source.setMipLevels(level,1);DkCopyBuf destination{staging.getGpuAddr(),0,0};DkImageRect rect{0,0,0,w,h,1};
    commands_.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);commands_.copyImageToBuffer(source,rect,destination);
    commands_.barrier(DkBarrier_Full,DkInvalidateFlags_L2Cache);dkQueueSubmitCommands(queue_,commands_.finishList());dkQueueFlush(queue_);dkQueueWaitIdle(queue_);
    if(dkQueueIsInErrorState(queue_))throw std::runtime_error("offscreen readback queue error");commands_.clear();std::memcpy(result.data(),staging.getCpuAddr(),bytes);return result;
}
void DekoTextureStore::WriteImage(const dk::Image& image,std::uint32_t w,std::uint32_t h,std::uint32_t pixel_bytes,std::span<const std::byte> bytes,std::uint32_t level){
    const std::uint64_t size=std::uint64_t(w)*h*pixel_bytes;if(!w || !h || w>8192 || h>8192 || (pixel_bytes!=1 && pixel_bytes!=2 && pixel_bytes!=4) || size>64*1024*1024 || size!=bytes.size())throw std::invalid_argument("offscreen writeback extent");
    if(dkQueueIsInErrorState(queue_))throw std::runtime_error("offscreen writeback queue lost");
    auto staging=dk::MemBlockMaker{device_,Align(size)}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();if(!staging)throw std::bad_alloc();std::memcpy(staging.getCpuAddr(),bytes.data(),size);
    commands_.clear();commands_.addMemory(command_memory_,0,DK_MEMBLOCK_ALIGNMENT);dk::ImageView destination{image};destination.setMipLevels(level,1);DkCopyBuf source{staging.getGpuAddr(),0,0};DkImageRect rect{0,0,0,w,h,1};
    commands_.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);commands_.copyBufferToImage(source,destination,rect);
    commands_.barrier(DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_L2Cache);dkQueueSubmitCommands(queue_,commands_.finishList());dkQueueFlush(queue_);dkQueueWaitIdle(queue_);
    if(dkQueueIsInErrorState(queue_))throw std::runtime_error("offscreen writeback queue error");commands_.clear();
}

}
