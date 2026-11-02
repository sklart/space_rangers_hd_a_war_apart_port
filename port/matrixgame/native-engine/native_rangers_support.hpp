// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace srhd_awa::matrixgame {
namespace platform3g {class Device;}
struct RangersSupportFacts {
 bool query_succeeded{};
 std::uint32_t simultaneous_textures{},max_texture_width{},streams{};
};
// Same ordering/codes as original MatrixGame::Support, with facts from the
// actual registered renderer. A query is not an Init or a memory-budget claim.
constexpr std::int32_t ClassifyRangersSupport(RangersSupportFacts facts)noexcept{
 if(!facts.query_succeeded)return 1;
 if(facts.simultaneous_textures<2 || facts.max_texture_width<2048)return 3;
 if(!facts.streams)return 2;
 return 0;
}
RangersSupportFacts QueryRangersSupport(platform3g::Device&);
}
