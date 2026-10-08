#pragma once

#include "gai_cpu.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace srhd_awa::platform::gai_frame_sequence_cpu {

// Parses one or more [delay,first-last] ranges. Never trusts range expansion.
bool Parse(std::string_view text, std::int32_t source_frame_count,
           gai_cpu::GaiSequence* out, std::string* error = nullptr);

}  // namespace srhd_awa::platform::gai_frame_sequence_cpu
