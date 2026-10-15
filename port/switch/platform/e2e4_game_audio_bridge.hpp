#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace srhd_awa::platform::e2e4_game_audio {

struct Format {
    std::uint16_t tag;
    std::uint16_t channels;
    std::uint32_t rate;
    std::uint32_t average_bytes_per_second;
    std::uint16_t block_align;
    std::uint16_t bits;
};

struct StreamStats {
    bool valid;
    std::uint64_t played_frames;
    std::uint64_t underrun_frames;
    std::uint64_t rejected_writes;
    std::uint64_t callback_count;
    std::uint64_t callback_max_us;
    std::uint64_t callback_gap_max_us;
    std::uint32_t ready_chunks;
};

enum class DeferredWriteResult { None, Written, Pending, Unavailable };

bool Open(std::string* error);
void Close();
bool PrepareEffect(void* owner, const void* data, std::size_t bytes,
                   const Format& format);
bool PrepareStream(void* owner, std::size_t chunk_bytes, const Format& format);
bool WriteStreamChunk(void* owner, std::size_t chunk, const void* data,
                      std::size_t bytes);
bool DeferStreamChunk(void* owner, std::size_t chunk, const void* data,
                      std::size_t bytes);
DeferredWriteResult RetryDeferredStreamChunk(void* owner);
bool Play(void* owner, bool looping, float gain);
void Stop(void* owner);
void Clear(void* owner);
bool IsPlaying(void* owner);
bool IsStreamPrimed(void* owner);
void SetGain(void* owner, float gain);
void SetPan(void* owner, float pan);
// Returns -1 for no completion; otherwise original event numbers 1, 2, 0
// for completed chunks 0, 1, 2 respectively.
int PollCompletion(void* owner);
StreamStats GetStreamStats(void* owner);

} // namespace srhd_awa::platform::e2e4_game_audio
