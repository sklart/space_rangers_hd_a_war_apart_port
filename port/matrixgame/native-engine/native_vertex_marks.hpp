// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace srhd_awa::matrixgame::platform3g {
// All indices must have been range-checked before MarkFirst. Scratch storage
// is local to the draw and never retains buffer or GPU owners.
class NativeVertexMarks {
    static constexpr std::size_t InlineWords=8;
    std::array<std::uint64_t,InlineWords> inline_{};
    std::vector<std::uint64_t> large_;
    std::span<std::uint64_t> words_;
public:
    explicit NativeVertexMarks(std::size_t count){
        const auto words=count/64+(count%64!=0);
        if(words>InlineWords){large_.resize(words);words_=large_;}
        else words_=std::span{inline_}.first(words);
    }
    NativeVertexMarks(const NativeVertexMarks&)=delete;
    NativeVertexMarks& operator=(const NativeVertexMarks&)=delete;
    NativeVertexMarks(NativeVertexMarks&&)=delete;
    NativeVertexMarks& operator=(NativeVertexMarks&&)=delete;
    bool MarkFirst(std::size_t index)noexcept{
        auto& word=words_[index/64];const auto mask=std::uint64_t(1)<<(index%64);
        if(word&mask)return false;word|=mask;return true;
    }
};
}
