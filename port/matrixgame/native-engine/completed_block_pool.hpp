// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
namespace srhd_awa::matrixgame::platform3g {
// Only the queue-completion owner may return a block. Borrowers keep unique
// ownership until then. Size metadata is cached: searches never call the SDK.
template<class Block,std::size_t Slots=128>class CompletedBlockPool {
    static_assert(Slots>0);
    struct Slot {Block block;std::uint64_t size{};};
    std::array<Slot,Slots> blocks_{};
    std::uint64_t bytes_{},limit_;std::size_t cursor_{};
    std::size_t Find(std::uint64_t size,std::uint64_t maximum)const noexcept{
        std::size_t best=Slots;if(!size || maximum<size)return best;
        for(std::size_t i=0;i<Slots;++i){const auto n=blocks_[i].size;
            if(n>=size && n<=maximum && (best==Slots || n<blocks_[best].size)){
                best=i;if(n==size)break;
            }
        }return best;
    }
public:
    std::uint64_t hits{},misses{},evictions{},larger_hits{};
    explicit CompletedBlockPool(std::uint64_t limit=16ull*1024*1024):limit_(limit){}
    std::uint64_t BestFitSize(std::uint64_t size,std::uint64_t maximum)const noexcept{
        const auto i=Find(size,maximum);return i==Slots?0:blocks_[i].size;
    }
    Block Take(std::uint64_t size){return TakeAtLeast(size,size);}
    Block TakeAtLeast(std::uint64_t size,std::uint64_t maximum){
        const auto i=Find(size,maximum);if(i==Slots){++misses;return {};}
        auto& slot=blocks_[i];bytes_-=slot.size;++hits;larger_hits+=slot.size>size;slot.size=0;return std::move(slot.block);
    }
    void Put(Block block)noexcept{
        if(!block)return;
        const auto size=block.getSize();
        if(!size || size>limit_)return;
        // Fixed storage: retirement never allocates CPU memory. Rotate eviction
        // so a changed scene can populate new size classes rather than thrash.
        while(bytes_>limit_-size){
            auto& old=blocks_[cursor_++%Slots];if(old.size){bytes_-=old.size;old.block=Block{};old.size=0;++evictions;}
        }
        for(auto& slot:blocks_)if(!slot.size){slot.block=std::move(block);slot.size=size;bytes_+=size;return;}
        auto& old=blocks_[cursor_++%Slots];bytes_-=old.size;old.block=std::move(block);old.size=size;bytes_+=size;++evictions;
    }
    std::uint64_t Limit()const noexcept{return limit_;}
    std::uint64_t Bytes()const noexcept{return bytes_;}
    void Clear()noexcept{for(auto& slot:blocks_){slot.block=Block{};slot.size=0;}bytes_=0;}
};
}
