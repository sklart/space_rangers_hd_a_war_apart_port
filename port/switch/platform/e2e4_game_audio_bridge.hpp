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

bool Open(std::string* error);
void Close();
bool PrepareEffect(void* owner, const void* data, std::size_t bytes,
                   const Format& format);
bool PrepareStream(void* owner, std::size_t chunk_bytes, const Format& format);
bool WriteStreamChunk(void* owner, std::size_t chunk, const void* data,
                      std::size_t bytes);
bool Play(void* owner, bool looping, float gain);
void Stop(void* owner);
void Clear(void* owner);
bool IsPlaying(void* owner);
void SetGain(void* owner, float gain);
void SetPan(void* owner, float pan);
// Returns -1 for no completion; otherwise original event numbers 1, 2, 0
// for completed chunks 0, 1, 2 respectively.
int PollCompletion(void* owner);

} // namespace srhd_awa::platform::e2e4_game_audio
