// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
namespace srhd_awa::matrixgame::native_texture {
struct ResidencyExhausted:std::bad_alloc {};
// Suballocation metadata only. Capacity is supplied AFTER backing GPU memory
// has been allocated. Never derives a texture budget from process RAM.
class Residency {
public:
    struct Range {std::uint32_t offset,size;};
    explicit Residency(std::uint32_t bytes):capacity_(bytes),free_{{0,bytes}} {
        if(!bytes)throw std::invalid_argument("empty texture arena");
    }
    Range Allocate(std::uint32_t size,std::uint32_t alignment) {
        if(!size || !alignment || (alignment&(alignment-1)))throw std::invalid_argument("texture allocation layout");
        for(std::size_t i=0;i<free_.size();++i){
            auto r=free_[i];const std::uint64_t begin=(std::uint64_t(r.offset)+alignment-1)&~std::uint64_t(alignment-1);
            const auto end=std::uint64_t(r.offset)+r.size;
            if(begin>end || size>end-begin)continue;
            // Build the replacement before publication; allocation failure
            // preserves all free ranges and the old resident image.
            auto next=free_;next.reserve(free_.size()+live_.size()+3);next.erase(next.begin()+i);
            if(begin>r.offset)next.push_back({r.offset,std::uint32_t(begin-r.offset)});
            if(begin+size<end)next.push_back({std::uint32_t(begin+size),std::uint32_t(end-begin-size)});
            std::sort(next.begin(),next.end(),[](auto a,auto b){return a.offset<b.offset;});
            Range result{std::uint32_t(begin),size};
            live_.push_back(result); // May fail before the free-list commit.
            free_.swap(next);return result;
        }
        throw ResidencyExhausted();
    }
    void Release(Range r) {
        if(!r.size || std::uint64_t(r.offset)+r.size>capacity_)throw std::logic_error("invalid texture arena release");
        auto it=std::find_if(live_.begin(),live_.end(),[r](auto l){return l.offset==r.offset && l.size==r.size;});
        if(it==live_.end())throw std::logic_error("texture arena double/partial release");
        // Capacity was reserved before allocation publication. Destruction
        // and rollback need no heap allocation, even during OOM unwinding.
        free_.push_back(r);live_.erase(it);std::sort(free_.begin(),free_.end(),[](auto a,auto b){return a.offset<b.offset;});
        std::size_t count=0;
        for(auto f:free_){if(count && std::uint64_t(free_[count-1].offset)+free_[count-1].size==f.offset)free_[count-1].size+=f.size;else free_[count++]=f;}
        free_.resize(count);
    }
    std::uint64_t Free()const noexcept {std::uint64_t n=0;for(auto r:free_)n+=r.size;return n;}
    std::uint32_t Largest()const noexcept {std::uint32_t n=0;for(auto r:free_)n=std::max(n,r.size);return n;}
    std::uint32_t Capacity()const noexcept{return capacity_;}
private:std::uint32_t capacity_;std::vector<Range> free_,live_;
};
}
