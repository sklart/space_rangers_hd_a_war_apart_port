#include "e2e4_game_audio_bridge.hpp"

#include "e2e4_sdl_audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace srhd_awa::platform::e2e4_game_audio {
namespace {

using e2e4_audio::Bus;
using e2e4_audio::Pcm;
using e2e4_audio::SdlOutput;

struct Buffer {
    std::shared_ptr<const Pcm> pcm;
    std::uint64_t voice = 0;
    std::uint64_t stream = 0;
    std::array<std::uint32_t, 3> pending{};
    std::size_t next_completion = 0;
    float gain = 1.0f;
    float pan = 0.0f;
    std::vector<std::uint8_t> deferred_pcm;
    std::size_t deferred_chunk = 0;
};

std::mutex lock;
SdlOutput output;
std::unordered_map<void*, Buffer> buffers;

bool ValidFormat(const Format& format) {
    const auto align = static_cast<std::uint32_t>(format.channels) * (format.bits / 8);
    return format.tag == 1 && (format.channels == 1 || format.channels == 2) &&
        (format.bits == 8 || format.bits == 16) && format.rate > 0 &&
        format.rate <= 192000 && format.block_align == align &&
        format.average_bytes_per_second == format.rate * align;
}

void ClearLocked(void* owner) {
    const auto it = buffers.find(owner);
    if (it == buffers.end()) return;
    if (it->second.voice) output.Stop(it->second.voice);
    if (it->second.stream) output.CloseStream(it->second.stream);
    buffers.erase(it);
    output.CollectFinished();
}

} // namespace

bool Open(std::string* error) {
    std::lock_guard guard(lock);
    return output.Open(error);
}

void Close() {
    std::lock_guard guard(lock);
    for (auto& [owner, buffer] : buffers) {
        (void)owner;
        if (buffer.voice) output.Stop(buffer.voice);
        if (buffer.stream) output.CloseStream(buffer.stream);
    }
    buffers.clear();
    output.Close();
}

bool PrepareEffect(void* owner, const void* data, std::size_t bytes,
                   const Format& format) {
    if (!owner || !data || !ValidFormat(format) || !bytes ||
        bytes > 32 * 1024 * 1024 || bytes % format.block_align) return false;
    auto pcm = std::make_shared<Pcm>();
    pcm->rate = format.rate;
    pcm->channels = static_cast<std::uint8_t>(format.channels);
    pcm->bits = static_cast<std::uint8_t>(format.bits);
    pcm->bytes.resize(bytes);
    std::memcpy(pcm->bytes.data(), data, bytes);
    std::lock_guard guard(lock);
    if (!output.IsOpen()) return false;
    ClearLocked(owner);
    buffers[owner].pcm = std::move(pcm);
    return true;
}

bool PrepareStream(void* owner, std::size_t chunk_bytes, const Format& format) {
    if (!owner || !ValidFormat(format)) return false;
    std::lock_guard guard(lock);
    if (!output.IsOpen()) return false;
    ClearLocked(owner);
    const auto handle = output.CreateStream(chunk_bytes, format.rate,
        static_cast<std::uint8_t>(format.channels),
        static_cast<std::uint8_t>(format.bits));
    if (!handle) return false;
    buffers[owner].stream = handle;
    return true;
}

bool WriteStreamChunk(void* owner, std::size_t chunk, const void* data,
                      std::size_t bytes) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    return it != buffers.end() && it->second.stream &&
        output.WriteStreamChunk(it->second.stream, chunk, data, bytes);
}

bool DeferStreamChunk(void* owner, std::size_t chunk, const void* data,
                      std::size_t bytes) {
    if (chunk >= 3 || !data || !bytes || bytes > 2 * 1024 * 1024 / 3) return false;
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end() || !it->second.stream ||
        !it->second.deferred_pcm.empty() ||
        !output.IsStreamPlaying(it->second.stream)) return false;
    it->second.deferred_pcm.assign(static_cast<const std::uint8_t*>(data),
                                   static_cast<const std::uint8_t*>(data) + bytes);
    it->second.deferred_chunk = chunk;
    return true;
}

DeferredWriteResult RetryDeferredStreamChunk(void* owner) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end() || !it->second.stream) return DeferredWriteResult::Unavailable;
    auto& buffer = it->second;
    if (buffer.deferred_pcm.empty()) return DeferredWriteResult::None;
    if (!output.IsStreamPlaying(buffer.stream)) return DeferredWriteResult::Unavailable;
    if (!output.WriteStreamChunk(buffer.stream, buffer.deferred_chunk,
                                 buffer.deferred_pcm.data(), buffer.deferred_pcm.size()))
        return DeferredWriteResult::Pending;
    buffer.deferred_pcm.clear();
    return DeferredWriteResult::Written;
}

bool Play(void* owner, bool looping, float gain) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end()) return false;
    auto& buffer = it->second;
    buffer.gain = std::isfinite(gain) ? std::clamp(gain, 0.0f, 1.0f) : 0.0f;
    if (buffer.stream) {
        output.SetStreamGain(buffer.stream, buffer.gain);
        return output.StartStream(buffer.stream);
    }
    if (!buffer.pcm) return false;
    if (buffer.voice) output.Stop(buffer.voice);
    output.CollectFinished();
    buffer.voice = output.Add(buffer.pcm, Bus::effects, looping,
                              buffer.gain, buffer.pan);
    return buffer.voice != 0;
}

void Stop(void* owner) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end()) return;
    if (it->second.voice) output.Stop(it->second.voice);
    if (it->second.stream) output.StopStream(it->second.stream);
}

void Clear(void* owner) {
    std::lock_guard guard(lock);
    ClearLocked(owner);
}

bool IsPlaying(void* owner) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end()) return false;
    if (it->second.stream) return output.IsStreamPlaying(it->second.stream);
    return it->second.voice && output.IsPlaying(it->second.voice);
}

bool IsStreamPrimed(void* owner) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    return it != buffers.end() && it->second.stream &&
        output.IsStreamPrimed(it->second.stream);
}

void SetGain(void* owner, float gain) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end()) return;
    it->second.gain = std::isfinite(gain) ? std::clamp(gain, 0.0f, 1.0f) : 0.0f;
    if (it->second.voice) output.SetGain(it->second.voice, it->second.gain);
    if (it->second.stream) output.SetStreamGain(it->second.stream, it->second.gain);
}

void SetPan(void* owner, float pan) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end()) return;
    it->second.pan = std::isfinite(pan) ? std::clamp(pan, -1.0f, 1.0f) : 0.0f;
    if (it->second.voice) output.SetPan(it->second.voice, it->second.pan);
}

int PollCompletion(void* owner) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end() || !it->second.stream) return -1;
    auto& buffer = it->second;
    const auto events = output.DrainStreamCompleted(buffer.stream);
    for (std::size_t i = 0; i < 3; ++i) {
        const auto available = UINT32_MAX - buffer.pending[i];
        buffer.pending[i] += std::min(available, events[i]);
    }
    if (!buffer.pending[buffer.next_completion]) return -1;
    const auto done = buffer.next_completion;
    --buffer.pending[done];
    buffer.next_completion = (done + 1) % 3;
    return static_cast<int>((done + 1) % 3);
}

StreamStats GetStreamStats(void* owner) {
    std::lock_guard guard(lock);
    const auto it = buffers.find(owner);
    if (it == buffers.end() || !it->second.stream) return {};
    const auto stats = output.GetStreamStats(it->second.stream);
    return {stats.valid, stats.played_frames, stats.underrun_frames,
            stats.rejected_writes, stats.callback_count, stats.callback_max_us,
            stats.callback_gap_max_us};
}

} // namespace srhd_awa::platform::e2e4_game_audio
