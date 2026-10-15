#pragma once

#include "types/Windows_group.hpp"

#include <cstdint>

namespace srhd_awa::platform::e2e_calendar {
void UtcFromUnixMilliseconds(std::int64_t milliseconds,
                             Windows::TSystemTime& output);
void GetSystemTime(Windows::TSystemTime& output);
void GetLocalTime(Windows::TSystemTime& output);
}
