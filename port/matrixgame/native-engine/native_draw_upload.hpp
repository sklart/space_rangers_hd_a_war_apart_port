// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <deko3d.hpp>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <utility>
namespace srhd_awa::matrixgame::platform3g {
// One owned GPU block per draw for the former independent vertex/index and
// descriptor blocks. Preserve page-sized slices, their budget and SDK alignment.
// A trailing page can also carry the uniforms. Image owners stay in DrawPacket.
class NativeDrawUpload {
public:
    static constexpr unsigned Slots=8;
    static constexpr unsigned UniformBytes=DK_MEMBLOCK_ALIGNMENT;
    static constexpr unsigned ArenaUniformBytes=10*DK_UNIFORM_BUF_ALIGNMENT;
    struct Layout {
        std::uint32_t vertices{},indices{},samplers{},images{},bytes{},uniforms{};
        bool operator==(const Layout&)const=default;
    };
    static Layout Plan(std::uint64_t vertex_bytes,std::uint64_t index_bytes,bool with_uniforms=false){
        auto align=[](std::uint64_t bytes){
            if(!bytes || bytes>UINT32_MAX-(DK_MEMBLOCK_ALIGNMENT-1))throw std::length_error("native draw upload extent");
            return (bytes+DK_MEMBLOCK_ALIGNMENT-1)&~std::uint64_t(DK_MEMBLOCK_ALIGNMENT-1);
        };
        const auto indices=align(vertex_bytes);
        const auto samplers=indices+(index_bytes?align(index_bytes):0);
        const auto images=samplers+align(Slots*sizeof(DkSamplerDescriptor));
        const auto end_images=images+align(Slots*sizeof(DkImageDescriptor));
        const auto bytes=end_images+(with_uniforms?UniformBytes:0);
        if(bytes>UINT32_MAX)throw std::length_error("native draw upload block extent");
        return {0,std::uint32_t(indices),std::uint32_t(samplers),std::uint32_t(images),std::uint32_t(bytes),
            with_uniforms?std::uint32_t(end_images):0};
    }
    // An arena is already backed by one page-aligned SDK block. Its internal
    // resources need 256-byte alignment, not a separate 4 KiB page each.
    static Layout PlanArena(std::uint64_t vertex_bytes,std::uint64_t index_bytes,bool with_uniforms=false){
        if(!vertex_bytes || vertex_bytes>UINT32_MAX || index_bytes>UINT32_MAX)
            throw std::length_error("native arena upload extent");
        constexpr std::uint64_t unit=DK_UNIFORM_BUF_ALIGNMENT;
        const auto align=[](std::uint64_t n,std::uint64_t a){return (n+a-1)&~(a-1);};
        const auto indices=align(vertex_bytes,unit);
        const auto samplers=align(indices+index_bytes,unit);
        const auto images=align(samplers+Slots*sizeof(DkSamplerDescriptor),unit);
        const auto end_images=align(images+Slots*sizeof(DkImageDescriptor),unit);
        const auto bytes=align(end_images+(with_uniforms?ArenaUniformBytes:0),DK_MEMBLOCK_ALIGNMENT);
        if(bytes>UINT32_MAX)throw std::length_error("native arena upload block extent");
        return {0,std::uint32_t(indices),std::uint32_t(samplers),std::uint32_t(images),std::uint32_t(bytes),
                with_uniforms?std::uint32_t(end_images):0};
    }
    NativeDrawUpload()=default;
    NativeDrawUpload(dk::UniqueMemBlock memory,std::span<const std::byte> vertices,std::span<const std::byte> indices,bool with_uniforms=false)
        :layout_(Plan(vertices.size(),indices.size(),with_uniforms)),memory_(std::move(memory)),uniform_bytes_(with_uniforms?UniformBytes:0){
        if(!memory_)throw std::bad_alloc();
        capacity_=memory_.getSize();
        if(capacity_<layout_.bytes)throw std::invalid_argument("short native draw upload block");
        cpu_=static_cast<std::byte*>(memory_.getCpuAddr());gpu_=memory_.getGpuAddr();
        if(!cpu_)throw std::invalid_argument("native upload memory is not CPU accessible");
        std::memcpy(cpu_,vertices.data(),vertices.size());
        if(!indices.empty())std::memcpy(cpu_+layout_.indices,indices.data(),indices.size());
        std::memset(cpu_+layout_.images,0,Slots*sizeof(DkImageDescriptor));
    }
    // Borrowed slice: the batch arena retains the SDK block until queue completion.
    NativeDrawUpload(std::byte* cpu,DkGpuAddr gpu,std::uint32_t bytes,std::span<const std::byte> vertices,
                     std::span<const std::byte> indices,bool with_uniforms=false,bool clear_images=true)
        :layout_(PlanArena(vertices.size(),indices.size(),with_uniforms)),capacity_(bytes),uniform_bytes_(with_uniforms?ArenaUniformBytes:0),cpu_(cpu),gpu_(gpu){
        if(!cpu_ || capacity_<layout_.bytes || gpu_%DK_MEMBLOCK_ALIGNMENT)throw std::invalid_argument("invalid native arena upload slice");
        std::memcpy(cpu_,vertices.data(),vertices.size());
        if(!indices.empty())std::memcpy(cpu_+layout_.indices,indices.data(),indices.size());
        if(clear_images)ClearImages();
    }
    NativeDrawUpload(NativeDrawUpload&& other)noexcept
        :layout_(other.layout_),memory_(std::move(other.memory_)),capacity_(std::exchange(other.capacity_,0)),uniform_bytes_(std::exchange(other.uniform_bytes_,0)),
         cpu_(std::exchange(other.cpu_,nullptr)),gpu_(std::exchange(other.gpu_,0)){}
    NativeDrawUpload& operator=(NativeDrawUpload&& other){
        if(this!=&other){layout_=other.layout_;memory_=std::move(other.memory_);capacity_=std::exchange(other.capacity_,0);uniform_bytes_=std::exchange(other.uniform_bytes_,0);
            cpu_=std::exchange(other.cpu_,nullptr);gpu_=std::exchange(other.gpu_,0);}
        return *this;
    }
    NativeDrawUpload(const NativeDrawUpload&)=delete;
    NativeDrawUpload& operator=(const NativeDrawUpload&)=delete;
    void WriteImage(unsigned slot,const DkImageDescriptor& value){Write(layout_.images,slot,value);}
    void ClearImages(){if(!cpu_)throw std::logic_error("native draw upload lifetime ended");std::memset(cpu_+layout_.images,0,Slots*sizeof(DkImageDescriptor));}
    void WriteSampler(unsigned slot,const DkSamplerDescriptor& value){Write(layout_.samplers,slot,value);}
    DkGpuAddr Vertices(){return Address(layout_.vertices);}
    DkGpuAddr Indices(){return Address(layout_.indices);}
    DkGpuAddr Images(){return Address(layout_.images);}
    DkGpuAddr Samplers(){return Address(layout_.samplers);}
    std::span<std::byte> UniformStorage(){
        if(!cpu_ || !layout_.uniforms)throw std::logic_error("native draw has no uniform storage");
        return {cpu_+layout_.uniforms,uniform_bytes_};
    }
    DkGpuAddr UniformAddress(){
        if(!layout_.uniforms)throw std::logic_error("native draw has no uniform storage");
        return Address(layout_.uniforms);
    }
    std::uint32_t StorageBytes()const noexcept{return cpu_?capacity_:0;}
    dk::UniqueMemBlock TakeCompletedMemory()noexcept{capacity_=0;uniform_bytes_=0;cpu_=nullptr;gpu_=0;return std::move(memory_);}
private:
    static_assert(UniformBytes%DK_UNIFORM_BUF_ALIGNMENT==0);
    static_assert(DK_UNIFORM_BUF_ALIGNMENT%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
    static_assert(DK_UNIFORM_BUF_ALIGNMENT%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
    static_assert(DK_MEMBLOCK_ALIGNMENT%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
    static_assert(DK_MEMBLOCK_ALIGNMENT%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
    static_assert(sizeof(DkImageDescriptor)%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
    static_assert(sizeof(DkSamplerDescriptor)%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
    Layout layout_{};
    dk::UniqueMemBlock memory_;
    std::uint32_t capacity_{};
    std::uint32_t uniform_bytes_{};
    std::byte* cpu_{};DkGpuAddr gpu_{};
    template<class T>void Write(std::uint32_t offset,unsigned slot,const T& value){
        if(!cpu_ || slot>=Slots)throw std::out_of_range("native draw descriptor slot/lifetime");
        std::memcpy(cpu_+offset+slot*sizeof(T),&value,sizeof(value));
    }
    DkGpuAddr Address(std::uint32_t offset){
        if(!cpu_)throw std::logic_error("native draw upload lifetime ended");
        return gpu_+offset;
    }
};
}
