#pragma once

#include <cstdio>
#include <filesystem>

namespace srhd_awa::platform::e2e_stage {
inline constexpr const char* kRoot = "sdmc:/switch/space-rangers-hd-a-war-apart";

inline void Log(const char* stage) {
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::path(kRoot) / "logs", error);
  if (error) return;
  const auto path = std::filesystem::path(kRoot) / "logs/e2e.log";
  if (auto* file = std::fopen(path.generic_string().c_str(), "a")) {
    std::fprintf(file, "[E2E] %s\n", stage);
    std::fclose(file);
  }
}
}  // namespace srhd_awa::platform::e2e_stage
