// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
namespace srhd_awa::matrixgame::platform3g {
struct ProfileTotals {
    std::uint64_t calls{},us{},maximum_us{};
    void Add(std::uint64_t elapsed)noexcept{
        ++calls;us+=elapsed;if(elapsed>maximum_us)maximum_us=elapsed;
    }
};
class ProfileScope {
    using Clock=std::chrono::steady_clock;
    ProfileTotals& totals_;ProfileTotals* detail_;Clock::time_point start_=Clock::now();bool stopped_{};std::uint64_t elapsed_{};
public:
    explicit ProfileScope(ProfileTotals& totals,ProfileTotals* detail=nullptr):totals_(totals),detail_(detail){}
    std::uint64_t Stop()noexcept{
        if(!stopped_){
            stopped_=true;
            elapsed_=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start_).count();
            totals_.Add(elapsed_);if(detail_)detail_->Add(elapsed_);
        }
        return elapsed_;
    }
    ~ProfileScope(){Stop();}
};
// Keep initial context and logarithmic milestones; actual failures are always
// recorded separately. Never applied to required execution or error handling.
inline bool SamplePressure(std::uint64_t count)noexcept{return count<=4 || (count && !(count&(count-1)));}
}
