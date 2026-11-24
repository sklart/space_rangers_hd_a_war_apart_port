// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
namespace srhd_awa::matrixgame::platform3g {
inline constexpr std::uint64_t NativeArenaLargeBytes=8ull*1024*1024;
inline constexpr std::uint64_t NativeArenaSmallBytes=4ull*1024*1024;
// One SDK block per in-flight batch. The owner must call Retire only after
// successful GPU completion or after closing the failed GPU channel.
template<class Block>class NativeGpuUploadArena {
    Block block_{};
    std::uint64_t used_{},capacity_{};
    std::byte* cpu_{};std::uint64_t gpu_{};
public:
    static constexpr std::uint64_t DefaultBytes=NativeArenaLargeBytes;
    static constexpr std::uint64_t FallbackBytes=NativeArenaSmallBytes;
    bool HasBlock()const noexcept{return bool(block_);}
    std::uint64_t Used()const noexcept{return used_;}
    std::uint64_t Capacity()const noexcept{return capacity_;}
    bool Fits(std::uint64_t bytes)const noexcept{return block_ && bytes && used_<=Capacity() && bytes<=Capacity()-used_;}
    template<class Factory>bool Ensure(std::uint64_t bytes,std::uint64_t budget,Factory&& make,std::uint64_t preferred=DefaultBytes){
        if(!bytes || bytes>budget)throw std::length_error("GPU upload arena size");
        if(block_)throw std::logic_error("GPU upload arena already allocated");
        if(preferred!=DefaultBytes && preferred!=FallbackBytes)throw std::invalid_argument("GPU upload arena preference");
        const auto requested=bytes>preferred?bytes:(budget>=preferred?preferred:budget);
        auto allocate=[&](std::uint64_t size){auto result=make(size);if(!result)throw std::bad_alloc();return std::move(result);};
        Block block;bool fallback=false;
        try{block=allocate(requested);}
        catch(const std::bad_alloc&){
            if(requested<=FallbackBytes || bytes>FallbackBytes)throw;
            block=allocate(FallbackBytes);fallback=true;
        }
        const auto capacity=block.getSize();auto* cpu=static_cast<std::byte*>(block.getCpuAddr());
        const auto gpu=block.getGpuAddr();
        const auto minimum=fallback?FallbackBytes:requested;
        if(capacity<minimum || capacity>budget || !cpu || gpu>std::numeric_limits<std::uint64_t>::max()-capacity)
            throw std::runtime_error("GPU upload arena allocation failed");
        capacity_=capacity;cpu_=cpu;gpu_=gpu;block_=std::move(block);used_=0;return fallback;
    }
    struct Slice {std::byte* cpu;std::uint64_t gpu,bytes;};
    Slice Append(std::uint64_t bytes,std::uint64_t alignment){
        if(!alignment || (alignment&(alignment-1)) || used_%alignment || bytes%alignment || !Fits(bytes))
            throw std::length_error("GPU upload arena slice");
        Slice slice{cpu_+used_,gpu_+used_,bytes};
        used_+=bytes;return slice;
    }
    Block Retire()noexcept{used_=0;capacity_=0;cpu_=nullptr;gpu_=0;return std::move(block_);}
};
}
