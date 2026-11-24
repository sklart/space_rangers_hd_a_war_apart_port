// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_gpu_upload_arena.hpp"
namespace srhd_awa::matrixgame::platform3g {
// After an allocation fallback, avoid retrying the larger allocation for a
// bounded number of arena lifetimes. A later attempt can recover automatically.
class NativeArenaPreference {
    unsigned cooldown_{};
public:
    static constexpr unsigned RetryAfter=64;
    std::uint64_t Preferred()const noexcept{return cooldown_?NativeArenaSmallBytes:NativeArenaLargeBytes;}
    void CompletedAllocation(bool fallback)noexcept{if(fallback)cooldown_=RetryAfter;else if(cooldown_)--cooldown_;}
};
}
