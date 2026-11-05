// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_draw_batch.hpp"
#include "native_profile.hpp"
#include <algorithm>
#include <array>

namespace srhd_awa::matrixgame::platform3g {
// Limit causes can coincide. Assign each wait to one combined cause so the
// partition reconciles with batch_gpu_wait.
enum class BatchWaitCause : unsigned {
    CommandSpace=4, Readback=8, Writeback, Initialization, Count
};
inline BatchWaitCause BatchLimitCause(unsigned draw_limits,bool command_space)noexcept{
    return static_cast<BatchWaitCause>(draw_limits | (command_space?unsigned(BatchWaitCause::CommandSpace):0u));
}
inline constexpr std::array BatchWaitNames{
    "", "draw_count", "upload_bytes", "draw_count+upload_bytes", "command_space",
    "draw_count+command_space", "upload_bytes+command_space", "all_limits",
    "readback", "writeback", "init_completion"
};
static_assert(BatchWaitNames.size()==unsigned(BatchWaitCause::Count));

struct BatchWaitProfile {
    static constexpr std::array<std::uint64_t,4> BucketUpperUs{250,1000,4000,16000};
    ProfileTotals wait;
    std::array<std::uint64_t,BucketUpperUs.size()+1> histogram{};
    std::uint64_t draws_max{},upload_bytes_max{},command_backing_bytes_max{},command_spills_max{};

    void Record(std::uint64_t us,std::uint64_t draws,std::uint64_t upload_bytes,
                std::uint64_t command_backing_bytes,std::uint64_t command_spills)noexcept{
        wait.Add(us);
        const auto bucket=std::lower_bound(BucketUpperUs.begin(),BucketUpperUs.end(),us)-BucketUpperUs.begin();
        ++histogram[bucket];
        draws_max=std::max(draws_max,draws);
        upload_bytes_max=std::max(upload_bytes_max,upload_bytes);
        command_backing_bytes_max=std::max(command_backing_bytes_max,command_backing_bytes);
        command_spills_max=std::max(command_spills_max,command_spills);
    }
};
}
