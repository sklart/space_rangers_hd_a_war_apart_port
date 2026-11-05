// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
namespace srhd_awa::matrixgame::platform3g {
struct ProfileTotals {std::uint64_t calls{},us{},maximum_us{};};
class ProfileScope {
    using Clock=std::chrono::steady_clock;
    ProfileTotals& totals_;ProfileTotals* detail_;Clock::time_point start_=Clock::now();bool stopped_{};
public:
    explicit ProfileScope(ProfileTotals& totals,ProfileTotals* detail=nullptr):totals_(totals),detail_(detail){}
    void Stop()noexcept{if(stopped_)return;stopped_=true;const auto us=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start_).count());auto add=[&](ProfileTotals& target){
        ++target.calls;target.us+=us;if(us>target.maximum_us)target.maximum_us=us;
    };add(totals_);if(detail_)add(*detail_);}
    ~ProfileScope(){Stop();}
};
// Keep initial context and logarithmic milestones; actual failures are always
// recorded separately. Never applied to required execution or error handling.
inline bool SamplePressure(std::uint64_t count)noexcept{return count<=4 || (count && !(count&(count-1)));}
}
