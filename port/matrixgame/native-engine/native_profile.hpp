// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>
namespace srhd_awa::matrixgame::platform3g {
struct ProfileTotals {std::uint64_t calls{},us{},maximum_us{};};
class ProfileScope {
    using Clock=std::chrono::steady_clock;
    ProfileTotals& totals_;Clock::time_point start_=Clock::now();bool stopped_{};
public:
    explicit ProfileScope(ProfileTotals& totals):totals_(totals){}
    void Stop()noexcept{if(stopped_)return;stopped_=true;const auto us=std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start_).count());++totals_.calls;totals_.us+=us;if(us>totals_.maximum_us)totals_.maximum_us=us;}
    ~ProfileScope(){Stop();}
};
// Keep initial context and logarithmic milestones; actual failures are always
// recorded separately. Never applied to required execution or error handling.
inline bool SamplePressure(std::uint64_t count)noexcept{return count<=4 || (count && !(count&(count-1)));}
}
