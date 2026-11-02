// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame::diagnostic {
// Cooperative checkpoints only. This cannot preempt a blocking SDK/GPU call.
class RunControl {
public:
    enum class PollResult {Continue,UserExit,Deadline};
    RunControl(std::uint64_t start_us,std::uint64_t timeout_us,std::uint64_t target_presents)
        :start_(start_us),last_(start_us),timeout_(timeout_us),target_(target_presents){
        if(!timeout_ || !target_)throw std::invalid_argument("diagnostic timeout/frame limit must be positive");
    }
    PollResult Poll(std::uint64_t now_us,bool exit_held){
        if(now_us<last_)throw std::logic_error("diagnostic monotonic clock moved backwards");last_=now_us;
        if(now_us-start_>=timeout_)return PollResult::Deadline;
        if(!exit_held)armed_=true;else if(armed_)return PollResult::UserExit;
        return PollResult::Continue;
    }
    bool Presented(std::uint64_t before,std::uint64_t after){
        if(after<before || after-before>UINT64_MAX-frames_)throw std::logic_error("diagnostic presentation counter invalid");
        frames_+=after-before;return frames_>=target_;
    }
    std::uint64_t Frames()const noexcept{return frames_;}
private:
    std::uint64_t start_,last_,timeout_,target_,frames_{};bool armed_{};
};
}
