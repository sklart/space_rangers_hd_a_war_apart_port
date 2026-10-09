#pragma once

#include "units/SysUtils.hpp"

#include <cstdint>

namespace srhd_awa::platform::e2e_file_search {

bool MatchPattern(const char* pattern, const char* name);

std::int32_t First(const char* pattern, std::int32_t attributes, SysUtils::TSearchRec& record);
std::int32_t Next(SysUtils::TSearchRec& record);
void Close(SysUtils::TSearchRec& record);

}  // namespace srhd_awa::platform::e2e_file_search
