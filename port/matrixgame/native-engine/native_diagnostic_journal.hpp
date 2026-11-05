// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>

namespace srhd_awa::matrixgame::diagnostic {
// The default functions perform real file IO. Overrides are for deterministic
// failure tests; a diagnostic launch uses SystemIO exclusively.
struct IO {
    std::size_t (*write)(const void*,std::size_t,FILE*);
    int (*flush)(FILE*);
    int (*close)(FILE*);
    int (*sync)(FILE*){};
};
IO SystemIO() noexcept;
struct JournalIOStats {
    std::uint64_t calls{},bytes{},us{};
    std::uint64_t write_us{},flush_calls{},flush_us{},sync_calls{},sync_us{};
    std::uint64_t durable_rows{};
};
class Journal {
public:
    static constexpr std::size_t BatchBytes=64*1024;
    static constexpr std::uint64_t BatchIntervalUs=1'000'000;
    Journal(const std::filesystem::path& log_root,std::string_view build_id,
            std::string_view source_sha256,IO io=SystemIO());
    ~Journal();
    Journal(const Journal&)=delete;
    const std::filesystem::path& Directory()const noexcept{return directory_;}
    const std::filesystem::path& Path()const noexcept{return path_;}
    const char* RunId()const noexcept{return run_id_.data();}
    bool Record(std::string_view event,std::string_view operation,
                std::string_view detail={},std::int64_t result=0)noexcept;
    bool Metric(std::string_view name,std::uint64_t value)noexcept;
    std::uint64_t Begin(std::string_view operation);
    bool End(std::uint64_t token,bool success,std::int64_t result=0,bool cancelled=false)noexcept;
    void BeginBatching();
    bool Checkpoint(bool force=false)noexcept;
    bool EndBatching()noexcept;
    bool Close()noexcept;
    bool Healthy()const noexcept;
    int Error()const noexcept;
    std::uint64_t Failures()const noexcept;
    JournalIOStats Statistics()const noexcept;
    void RequireHealthy()const;
private:
    struct Stage {std::array<char,97> name{};std::uint64_t token{},started{};};
    bool Emit(std::string_view event,std::string_view operation,std::string_view detail,
              std::int64_t result,std::uint64_t elapsed,std::uint64_t value=0)noexcept;
    bool Persist(const char* data,std::size_t size,std::uint64_t rows)noexcept;
    bool FlushPending()noexcept;
    mutable std::mutex mutex_;
    IO io_;FILE* file_{};bool closed_{};int error_{};
    std::filesystem::path directory_,path_;
    std::array<char,97> build_id_{},last_ok_{};
    std::array<char,65> source_sha_{},run_id_{};
    std::array<Stage,32> stack_{};unsigned depth_{};
    std::uint64_t sequence_{},token_{},started_{},failures_{};
    JournalIOStats io_stats_;
    std::unique_ptr<char[]> batch_;
    std::size_t pending_bytes_{};
    std::uint64_t pending_rows_{},pending_since_{};
    bool batching_{};
};
// Binding outlives device/queue/runtime, including all asynchronous callbacks.
// Only one diagnostic process session may be bound. Normal engine use has no
// binding and performs no diagnostic IO.
class Binding {
public:explicit Binding(Journal&);~Binding();Binding(const Binding&)=delete;
private:Journal* journal_;
};
class Operation {
public:explicit Operation(std::string_view);~Operation();Operation(const Operation&)=delete;
    void Fail(std::int64_t result)noexcept{failed_=true;result_=result;}
    void Cancel()noexcept{cancelled_=true;}
private:Journal* journal_{};std::uint64_t token_{};int exceptions_{};
    bool failed_{},cancelled_{};std::int64_t result_{};
};
void Record(std::string_view event,std::string_view operation,std::string_view detail={},std::int64_t result=0)noexcept;
void Metric(std::string_view name,std::uint64_t value)noexcept;
JournalIOStats CurrentIOStats()noexcept;
}
