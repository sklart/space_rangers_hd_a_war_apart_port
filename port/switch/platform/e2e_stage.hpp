#pragma once

#include <cstdio>
#include <chrono>
#include <cstdint>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace srhd_awa::platform::e2e_stage {
inline constexpr const char* kRoot = "sdmc:/switch/space-rangers-hd-a-war-apart";
inline std::mutex g_stage_mutex;
inline std::string g_current_stage = "entry";
inline std::string g_first_failure;
inline bool g_boot_ready = false;

// All access is protected by g_stage_mutex. Keep the stream open and buffered:
// opening/closing it for every diagnostic line forces SD I/O inside UI draws.
struct BufferedLog {
  FILE* file{};
  char buffer[16384]{};
  std::chrono::steady_clock::time_point last_flush{};
  ~BufferedLog() { if (file) std::fclose(file); }
};
inline BufferedLog g_log;
struct LogStats { std::uint64_t calls{}, write_us{}, flushes{}, flush_us{}; };
inline LogStats g_log_stats;
struct AudioTiming {
  std::atomic<std::uint64_t> calls{}, total_us{}, max_us{};
  void Add(std::uint64_t us) {
    calls.fetch_add(1, std::memory_order_relaxed);
    total_us.fetch_add(us, std::memory_order_relaxed);
    auto previous = max_us.load(std::memory_order_relaxed);
    while (previous < us && !max_us.compare_exchange_weak(previous, us, std::memory_order_relaxed)) {}
  }
};
inline AudioTiming audio_source, audio_decode;

inline LogStats Stats() {
  std::lock_guard lock(g_stage_mutex);
  return g_log_stats;
}

inline void Append(const char* prefix, const char* message) {
  const auto started = std::chrono::steady_clock::now();
  if (!g_log.file) {
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(kRoot) / "logs", error);
    if (error) return;
    const auto path = std::filesystem::path(kRoot) / "logs/e2e.log";
    g_log.file = std::fopen(path.generic_string().c_str(), "a");
    if (!g_log.file) return;
    std::setvbuf(g_log.file, g_log.buffer, _IOFBF, sizeof(g_log.buffer));
    g_log.last_flush = std::chrono::steady_clock::now();
  }
  std::fprintf(g_log.file, "%s%s\n", prefix, message);
  const auto now = std::chrono::steady_clock::now();
  const std::string_view view(message);
  // The host probe consumes these markers as its control protocol. Its local
  // pipe-like log must stay observable without adding delays to input scripts.
#if defined(E2E5_HOST_SDL_WINDOW)
  constexpr bool host_control_log = true;
#else
  constexpr bool host_control_log = false;
#endif
  if (host_control_log || view.starts_with("FAIL") || view.starts_with("BOOT") ||
      view.find("exception") != std::string_view::npos ||
      now - g_log.last_flush >= std::chrono::seconds(5)) {
    std::fflush(g_log.file);
    ++g_log_stats.flushes;
    g_log_stats.flush_us += std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - now).count();
    g_log.last_flush = now;
  }
  ++g_log_stats.calls;
  g_log_stats.write_us += std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started).count();
}

inline void Log(const char* stage) {
  std::lock_guard lock(g_stage_mutex);
  const std::string_view view(stage ? stage : "");
  if (view.starts_with("FAIL") && g_first_failure.empty())
    g_first_failure = view;
  if (!view.starts_with("FAIL") && !view.starts_with("build_git="))
    g_current_stage = view;
  Append("[E2E] ", stage ? stage : "");
}

inline void MarkBootReady() {
  std::lock_guard lock(g_stage_mutex);
  if (g_boot_ready || !g_first_failure.empty()) return;
  g_boot_ready = true;
  Append("[E2E] ", "BOOT READY");
}

inline bool BootReady() {
  std::lock_guard lock(g_stage_mutex);
  return g_boot_ready;
}

inline std::string FirstFailure() {
  std::lock_guard lock(g_stage_mutex);
  return g_first_failure;
}

inline void LogBootFailed() {
  const auto failure = FirstFailure();
  const auto detail = "BOOT FAILED first=" + (failure.empty() ? std::string("unknown") : failure);
  Log(detail.c_str());
}

inline std::string CurrentStage() {
  std::lock_guard lock(g_stage_mutex);
  return g_current_stage;
}

inline void LogWinApi(const char* message) {
  std::lock_guard lock(g_stage_mutex);
  Append("[E2E][WINAPI] ", message ? message : "");
}

inline void LogE2E3(const char* message) {
#if defined(E2E3_TRACE) && E2E3_TRACE
  std::lock_guard lock(g_stage_mutex);
  Append("[E2E3] ", message ? message : "");
#else
  (void)message;
#endif
}
}  // namespace srhd_awa::platform::e2e_stage
