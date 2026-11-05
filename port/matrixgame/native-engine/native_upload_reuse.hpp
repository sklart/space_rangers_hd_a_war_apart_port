// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame::platform3g {
struct UploadReusePlan {std::uint64_t capacity{},total{};};
// Inspect without acquiring memory. Recompute after a drain, which can return
// larger blocks. Physical capacity, including unused padding, consumes quota.
template<class Pool>
UploadReusePlan PlanUploadReuse(const Pool& pool,std::uint64_t requested,std::uint64_t budget){
    if(!requested || requested>budget)throw std::length_error("native single draw upload budget");
    const auto maximum=requested+std::min(requested,budget-requested);
    const auto fit=pool.BestFitSize(requested,maximum);
    const auto capacity=fit?fit:requested;
    return {capacity,capacity};
}
template<class Pool>UploadReusePlan PlanUploadReuse(const Pool& pool,std::uint64_t requested,std::uint64_t uniform,std::uint64_t budget){
    if(!requested || !uniform || uniform>=budget || requested>budget-uniform)throw std::length_error("native single draw upload budget");
    auto plan=PlanUploadReuse(pool,requested,budget-uniform);
    plan.total+=uniform;
    return plan;
}
}
