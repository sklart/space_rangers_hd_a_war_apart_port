// CPU endpoints for real SDK handles. No GPU execution or GPU latency inference.
#include "completed_block_pool.hpp"
#include "material_uniform_packet.hpp"
#include "native_draw_batch.hpp"
#include "native_draw_upload.hpp"
#include "native_upload_reuse.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace p = srhd_awa::matrixgame::platform3g;
namespace m = srhd_awa::matrixgame::materials;
namespace dk::detail {
struct Device {};
struct MemBlock {
    std::vector<std::byte> bytes;
    DkGpuAddr address;
};
}
namespace {
dk::detail::Device device;
std::map<DkGpuAddr, DkMemBlock> live;
DkGpuAddr next_address = 0x100000;
std::uint64_t creates{}, live_bytes{}, peak_bytes{};
bool allocation_failure{};
}
extern "C" DkMemBlock dkMemBlockCreate(const DkMemBlockMaker* maker) {
    if (allocation_failure) return nullptr;
    assert(maker->size && maker->size % DK_MEMBLOCK_ALIGNMENT == 0);
    auto* block = new dk::detail::MemBlock{
        std::vector<std::byte>(maker->size, std::byte{0xCD}), next_address};
    next_address += maker->size;
    assert(live.emplace(block->address, block).second);
    ++creates;
    live_bytes += maker->size;
    peak_bytes = std::max(peak_bytes, live_bytes);
    return block;
}
extern "C" void dkMemBlockDestroy(DkMemBlock memory) {
    assert(live.erase(memory->address) == 1);
    live_bytes -= memory->bytes.size();
    delete memory;
}
extern "C" void* dkMemBlockGetCpuAddr(DkMemBlock memory) { return memory->bytes.data(); }
extern "C" DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock memory) { return memory->address; }
extern "C" std::uint32_t dkMemBlockGetSize(DkMemBlock memory) { return std::uint32_t(memory->bytes.size()); }

namespace {
constexpr auto UniformBytes = m::uniform_packet::Bytes;
#ifndef FROZEN_R39
static_assert(UniformBytes == p::NativeDrawUpload::UniformBytes);
#endif
using Clock = std::chrono::steady_clock;

std::span<const std::byte> Read(DkGpuAddr address, std::size_t size) {
    auto block = live.upper_bound(address);
    assert(block != live.begin());
    --block;
    const auto offset = address - block->first;
    const auto& bytes = block->second->bytes;
    assert(offset <= bytes.size() && size <= bytes.size() - offset);
    return {bytes.data() + offset, size};
}

struct Driver {
    struct Packet {
        p::NativeDrawUpload upload;
        dk::UniqueMemBlock uniform;
        DkGpuAddr uniform_address;
        std::uint64_t sequence;
        std::size_t vertices, indices;
    };
    p::CompletedBlockPool<dk::UniqueMemBlock> pool;
    p::NativeFrameScope frame;
    p::NativeDrawBatch batch;
    std::vector<Packet> pending;
    std::uint64_t requested{}, reserved{}, draws{}, consumed{}, submissions{}, max_batch{};
    std::uint64_t hash = 1469598103934665603ull;
    bool combined, fail_wait{}, completed{};

    explicit Driver(bool fused) : combined(fused) { pending.reserve(64); }

    dk::UniqueMemBlock Memory(std::uint64_t size, std::uint64_t capacity) {
        auto cached = pool.TakeAtLeast(size, capacity);
        if (cached) return std::move(cached);
        auto fresh = dk::MemBlockMaker{&device, std::uint32_t(size)}.create();
        if (!fresh) throw std::bad_alloc();
        return dk::UniqueMemBlock{std::move(fresh)};
    }

    void Add(std::span<const std::byte> vertices, std::span<const std::byte> indices) {
#ifdef FROZEN_R39
        assert(!combined);
        const auto layout = p::NativeDrawUpload::Plan(vertices.size(), indices.size());
        const auto select = [&] {
            return p::PlanUploadReuse(pool, layout.bytes, UniformBytes, p::NativeDrawBatch::ByteLimit);
        };
#else
        const auto layout = p::NativeDrawUpload::Plan(vertices.size(), indices.size(), combined);
        const auto select = [&] {
            return combined ? p::PlanUploadReuse(pool, layout.bytes, p::NativeDrawBatch::ByteLimit)
                            : p::PlanUploadReuse(pool, layout.bytes, UniformBytes, p::NativeDrawBatch::ByteLimit);
        };
#endif
        auto plan = select();
        if (batch.NeedsDrain(plan.total)) {
            p::DrainNativeDrawBatch(frame, batch, *this);
            plan = select();
        }
#ifdef FROZEN_R39
        p::NativeDrawUpload upload{Memory(layout.bytes, plan.capacity), vertices, indices};
        auto uniform = Memory(UniformBytes, UniformBytes);
        const auto address = uniform.getGpuAddr();
        auto storage = std::span{static_cast<std::byte*>(uniform.getCpuAddr()), std::size_t(UniformBytes)};
#else
        p::NativeDrawUpload upload{Memory(layout.bytes, plan.capacity), vertices, indices, combined};
        dk::UniqueMemBlock uniform;
        if (!combined) uniform = Memory(UniformBytes, UniformBytes);
        const auto address = combined ? upload.UniformAddress() : uniform.getGpuAddr();
        auto storage = combined ? upload.UniformStorage()
            : std::span{static_cast<std::byte*>(uniform.getCpuAddr()), std::size_t(UniformBytes)};
        if (combined) assert(address == upload.Vertices() + layout.uniforms);
#endif
        m::Uniforms material{};
        m::VertexUniforms vertex{};
        m::PixelFogUniforms fog{};
        vertex.camera[0] = float(draws);
        m::uniform_packet::Write(storage, material, &vertex, fog, unsigned(draws % 2) + 1);
        for (unsigned i = 0; i < p::NativeDrawUpload::Slots; ++i) {
            DkSamplerDescriptor sampler{};
            std::memset(&sampler, int((draws + i) & 255), sizeof(sampler));
            upload.WriteSampler(i, sampler);
            if (i % 2) {
                DkImageDescriptor image{};
                std::memset(&image, int((draws ^ i) & 255), sizeof(image));
                upload.WriteImage(i, image);
            }
        }
        assert(upload.StorageBytes() + (combined ? 0 : UniformBytes) == plan.total);
        assert(address % DK_UNIFORM_BUF_ALIGNMENT == 0);
        pending.push_back({std::move(upload), std::move(uniform), address, draws++, vertices.size(), indices.size()});
        batch.Add(plan.total);
        requested += layout.bytes + (combined ? 0 : UniformBytes);
        reserved += plan.total;
        max_batch = std::max(max_batch, batch.Bytes());
    }

    void SubmitAndWait() {
        ++submissions;
        if (fail_wait) throw std::runtime_error("simulated queue failure");
        const auto mix = [&](DkGpuAddr address, std::size_t size) {
            for (auto byte : Read(address, size))
                hash = (hash ^ unsigned(byte)) * 1099511628211ull;
        };
        for (auto& packet : pending) {
            assert(packet.sequence == consumed++);
            mix(packet.upload.Vertices(), packet.vertices);
            if (packet.indices) mix(packet.upload.Indices(), packet.indices);
            mix(packet.upload.Samplers(), 8 * sizeof(DkSamplerDescriptor));
            mix(packet.upload.Images(), 8 * sizeof(DkImageDescriptor));
            mix(packet.uniform_address, UniformBytes);
        }
        completed = true;
    }

    void ReleaseCompleted() noexcept {
        assert(completed);
        for (auto& packet : pending) {
            pool.Put(packet.upload.TakeCompletedMemory());
            if (packet.uniform) pool.Put(std::move(packet.uniform));
        }
        pending.clear();
        completed = false;
    }
};

#ifndef FROZEN_R39
void Faults() {
    Driver d(true);
    d.frame.Acquire();
    d.frame.BeginScene();
    std::array<std::byte, 192> vertex{};
    allocation_failure = true;
    try { d.Add(vertex, {}); assert(false); } catch (const std::bad_alloc&) {}
    allocation_failure = false;
    assert(live.empty() && d.pending.empty() && !d.batch.Draws());
    d.Add(vertex, {});
    d.fail_wait = true;
    try { p::DrainNativeDrawBatch(d.frame, d.batch, d); assert(false); }
    catch (const std::runtime_error&) {}
    assert(d.pending.size() == 1 && d.batch.Draws() == 1 && d.pool.Bytes() == 0);
    assert(live.size() == 1 && Read(d.pending.front().uniform_address, UniformBytes).size() == UniformBytes);
    d.fail_wait = false;
    p::DrainNativeDrawBatch(d.frame, d.batch, d);
    assert(d.pending.empty() && d.batch.Draws() == 0 && d.pool.Bytes() == live_bytes);
}
#endif
}

int main(int argc, char** argv) {
    const bool combined = argc > 1 && std::strcmp(argv[1], "combined") == 0;
#ifndef FROZEN_R39
    Faults();
    assert(live.empty());
#endif
    creates = peak_bytes = 0;
    Driver d(combined);
    std::vector<std::byte> vertices(64 * 1024), indices(12 * 1024);
    for (std::size_t i = 0; i < vertices.size(); ++i) vertices[i] = std::byte((i * 13 + 5) & 255);
    for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = std::byte((i * 7 + 3) & 255);
    constexpr unsigned frames = 6, draws_per_frame = 1127;
    std::vector<long long> record_us;
    record_us.reserve(frames * draws_per_frame);
    const auto start = Clock::now();
    for (unsigned frame = 0; frame < frames; ++frame) {
        d.frame.Acquire();
        d.frame.BeginScene();
        for (unsigned draw = 0; draw < draws_per_frame; ++draw) {
            const unsigned v = 192 * (1 + ((draw * 71 + frame * 11) % 250));
            const unsigned i = draw % 7 ? 6 * (1 + draw * 13 % 1000) : 0;
            const auto begin = Clock::now();
            d.Add(std::span{vertices}.first(v), std::span{indices}.first(i));
            record_us.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count());
        }
        d.frame.EndScene();
        p::DrainNativeDrawBatch(d.frame, d.batch, d);
        d.frame.Submitted();
        d.frame.Presented();
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
    std::sort(record_us.begin(), record_us.end());
    assert(d.consumed == d.draws && d.draws == frames * draws_per_frame);
    std::printf("combined=%u draws=%llu submits=%llu creates=%llu hits=%llu misses=%llu requested=%llu reserved=%llu peak=%llu peak_batch=%llu hash=%llu total_us=%lld draw_p50_ns=%lld draw_p95_ns=%lld\n",
        unsigned(combined), (unsigned long long)d.draws, (unsigned long long)d.submissions,
        (unsigned long long)creates, (unsigned long long)d.pool.hits, (unsigned long long)d.pool.misses,
        (unsigned long long)d.requested, (unsigned long long)d.reserved, (unsigned long long)peak_bytes,
        (unsigned long long)d.max_batch, (unsigned long long)d.hash, (long long)us,
        record_us[record_us.size() / 2], record_us[record_us.size() * 95 / 100]);
    d.pool.Clear();
    assert(live.empty() && !live_bytes);
}
