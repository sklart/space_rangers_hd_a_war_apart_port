// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
namespace srhd_awa::matrixgame::platform3g {
// Only the queue-completion owner may return a block. Borrowers keep unique
// ownership until then. Same-size reuse preserves the existing batch budget.
template<class Block,std::size_t Slots=128>class CompletedBlockPool {
    static_assert(Slots>0);
    std::array<Block,Slots> blocks_{};
    std::uint64_t bytes_{},limit_;std::size_t cursor_{};
public:
    std::uint64_t hits{},misses{},evictions{};
    explicit CompletedBlockPool(std::uint64_t limit=16ull*1024*1024):limit_(limit){}
    Block Take(std::uint64_t size){
        for(auto& block:blocks_)if(block && block.getSize()==size){bytes_-=size;++hits;return std::move(block);}
        ++misses;return {};
    }
    void Put(Block block)noexcept{
        if(!block || block.getSize()>limit_)return;
        const auto size=block.getSize();
        // Fixed storage: retirement never allocates CPU memory. Rotate eviction
        // so a changed scene can populate new size classes rather than thrash.
        while(bytes_>limit_-size){
            auto& old=blocks_[cursor_++%Slots];if(old){bytes_-=old.getSize();old=Block{};++evictions;}
        }
        for(auto& slot:blocks_)if(!slot){slot=std::move(block);bytes_+=size;return;}
        auto& old=blocks_[cursor_++%Slots];bytes_-=old.getSize();old=std::move(block);bytes_+=size;++evictions;
    }
    std::uint64_t Bytes()const noexcept{return bytes_;}
    void Clear()noexcept{for(auto& block:blocks_)block=Block{};bytes_=0;}
};
}
