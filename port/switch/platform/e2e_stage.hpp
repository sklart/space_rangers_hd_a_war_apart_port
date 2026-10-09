#pragma once

#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace srhd_awa::platform::e2e_stage {
inline constexpr const char* kRoot = "sdmc:/switch/space-rangers-hd-a-war-apart";
inline std::mutex g_stage_mutex;
inline std::string g_current_stage = "entry";

inline void Append(const char* prefix, const char* message) {
  std::error_code error;
  std::filesystem::create_directories(std::filesystem::path(kRoot) / "logs", error);
  if (error) return;
  const auto path = std::filesystem::path(kRoot) / "logs/e2e.log";
  if (auto* file = std::fopen(path.generic_string().c_str(), "a")) {
    std::fprintf(file, "%s%s\n", prefix, message);
    std::fclose(file);
  }
}

inline void Log(const char* stage) {
  std::lock_guard lock(g_stage_mutex);
  const std::string_view view(stage ? stage : "");
  if (!view.starts_with("FAIL") && !view.starts_with("build_git="))
    g_current_stage = view;
  Append("[E2E] ", stage ? stage : "");
}

inline std::string CurrentStage() {
  std::lock_guard lock(g_stage_mutex);
  return g_current_stage;
}

inline void LogWinApi(const char* message) {
  std::lock_guard lock(g_stage_mutex);
  Append("[E2E][WINAPI] ", message ? message : "");
}
}  // namespace srhd_awa::platform::e2e_stage
