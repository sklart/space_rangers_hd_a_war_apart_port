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
// Uniforms and image owners remain in the caller's DrawPacket until completion.
class NativeDrawUpload {
public:
    static constexpr unsigned Slots=8;
    struct Layout {
        std::uint32_t vertices{},indices{},samplers{},images{},bytes{};
        bool operator==(const Layout&)const=default;
    };
    static Layout Plan(std::uint64_t vertex_bytes,std::uint64_t index_bytes){
        auto align=[](std::uint64_t bytes){
            if(!bytes || bytes>UINT32_MAX-(DK_MEMBLOCK_ALIGNMENT-1))throw std::length_error("native draw upload extent");
            return (bytes+DK_MEMBLOCK_ALIGNMENT-1)&~std::uint64_t(DK_MEMBLOCK_ALIGNMENT-1);
        };
        const auto indices=align(vertex_bytes);
        const auto samplers=indices+(index_bytes?align(index_bytes):0);
        const auto images=samplers+align(Slots*sizeof(DkSamplerDescriptor));
        const auto bytes=images+align(Slots*sizeof(DkImageDescriptor));
        if(bytes>UINT32_MAX)throw std::length_error("native draw upload block extent");
        return {0,std::uint32_t(indices),std::uint32_t(samplers),std::uint32_t(images),std::uint32_t(bytes)};
    }
    NativeDrawUpload()=default;
    NativeDrawUpload(dk::UniqueMemBlock memory,std::span<const std::byte> vertices,std::span<const std::byte> indices)
        :layout_(Plan(vertices.size(),indices.size())),memory_(std::move(memory)){
        if(!memory_)throw std::bad_alloc();
        capacity_=memory_.getSize();
        if(capacity_<layout_.bytes)throw std::invalid_argument("short native draw upload block");
        cpu_=static_cast<std::byte*>(memory_.getCpuAddr());gpu_=memory_.getGpuAddr();
        if(!cpu_)throw std::invalid_argument("native upload memory is not CPU accessible");
        std::memcpy(cpu_,vertices.data(),vertices.size());
        if(!indices.empty())std::memcpy(cpu_+layout_.indices,indices.data(),indices.size());
        std::memset(cpu_+layout_.images,0,Slots*sizeof(DkImageDescriptor));
    }
    NativeDrawUpload(NativeDrawUpload&&)=default;
    NativeDrawUpload& operator=(NativeDrawUpload&&)=default;
    NativeDrawUpload(const NativeDrawUpload&)=delete;
    NativeDrawUpload& operator=(const NativeDrawUpload&)=delete;
    void WriteImage(unsigned slot,const DkImageDescriptor& value){Write(layout_.images,slot,value);}
    void WriteSampler(unsigned slot,const DkSamplerDescriptor& value){Write(layout_.samplers,slot,value);}
    DkGpuAddr Vertices(){return Address(layout_.vertices);}
    DkGpuAddr Indices(){return Address(layout_.indices);}
    DkGpuAddr Images(){return Address(layout_.images);}
    DkGpuAddr Samplers(){return Address(layout_.samplers);}
    std::uint32_t StorageBytes()const noexcept{return memory_?capacity_:0;}
    dk::UniqueMemBlock TakeCompletedMemory()noexcept{capacity_=0;cpu_=nullptr;gpu_=0;return std::move(memory_);}
private:
    static_assert(DK_MEMBLOCK_ALIGNMENT%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
    static_assert(DK_MEMBLOCK_ALIGNMENT%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
    static_assert(sizeof(DkImageDescriptor)%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
    static_assert(sizeof(DkSamplerDescriptor)%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
    Layout layout_{};
    dk::UniqueMemBlock memory_;
    std::uint32_t capacity_{};
    std::byte* cpu_{};DkGpuAddr gpu_{};
    template<class T>void Write(std::uint32_t offset,unsigned slot,const T& value){
        if(!memory_ || slot>=Slots)throw std::out_of_range("native draw descriptor slot/lifetime");
        std::memcpy(cpu_+offset+slot*sizeof(T),&value,sizeof(value));
    }
    DkGpuAddr Address(std::uint32_t offset){
        if(!memory_)throw std::logic_error("native draw upload lifetime ended");
        return gpu_+offset;
    }
};
}
