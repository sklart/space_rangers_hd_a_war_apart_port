// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
namespace srhd_awa::matrixgame::platform3g::experimental {
// CPU-only planner: it does not own an SDK block or establish GPU completion.
// A caller must keep the underlying GPU block alive until a successful fence.
class DrawArena {
    enum class State {Recording, Submitted};
    std::uint64_t capacity_,alignment_,used_{};
    State state_{State::Recording};
public:
    DrawArena(std::uint64_t capacity,std::uint64_t alignment):capacity_(capacity),alignment_(alignment){
        if(!capacity || !alignment || (alignment&(alignment-1)))throw std::invalid_argument("arena alignment/capacity");
    }
    std::optional<std::uint64_t> Append(std::uint64_t bytes){
        if(state_!=State::Recording)throw std::logic_error("arena already submitted");
        if(!bytes)throw std::invalid_argument("empty arena allocation");
        if(used_>std::numeric_limits<std::uint64_t>::max()-(alignment_-1))return {};
        const auto offset=(used_+alignment_-1)&~(alignment_-1);
        if(offset>capacity_ || bytes>capacity_-offset)return {};
        used_=offset+bytes;return offset;
    }
    void Submit(){if(state_!=State::Recording || !used_)throw std::logic_error("arena submit state");state_=State::Submitted;}
    // A false result represents an error/timeout and must not release memory.
    bool Complete(bool fence_succeeded)noexcept{
        if(state_!=State::Submitted || !fence_succeeded)return false;
        used_=0;state_=State::Recording;return true;
    }
    std::uint64_t Used()const noexcept{return used_;}
};
}
