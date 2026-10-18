#include "e2e510_heap_reserve.hpp"
#include "windows.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

static bool deny_large = false;
extern "C" void* __real_malloc(std::size_t);
extern "C" void* __real_realloc(void*, std::size_t);
extern "C" void* __wrap_malloc(std::size_t bytes) {
  return deny_large && bytes >= 65536 ? nullptr : __real_malloc(bytes);
}
extern "C" void* __wrap_realloc(void* memory, std::size_t bytes) {
  return deny_large && bytes >= 65536 ? nullptr : __real_realloc(memory, bytes);
}
int main() {
  using namespace srhd_awa::platform::heap_reserve;
  constexpr std::size_t capacity = 128u * 1024u * 1024u;
  void* backing = __real_malloc(capacity);
  assert(backing);
  arena.Attach(backing, capacity);
  Initialize();
  const auto initial = arena.LargestFree();
  auto heap = pas::win::heap_create(0, 0, 0);
  auto* old = static_cast<unsigned char*>(pas::win::heap_alloc(heap, 0, 1024));
  std::memset(old, 73, 1024);
  deny_large = true;
  // Same allocation size as the hardware failure, after ordinary realloc fails.
  auto* recovered = static_cast<unsigned char*>(pas::win::heap_realloc(heap, 8, old, 13407092));
  assert(recovered && arena.Owns(recovered));
  for (unsigned i = 0; i < 1024; ++i) assert(recovered[i] == 73);
  for (unsigned i = 1024; i < 13407092; ++i) assert(recovered[i] == 0);
  auto* second = pas::win::heap_alloc(heap, 0, 11296506);
  auto* third = pas::win::heap_alloc(heap, 0, 4200097);
  assert(second && third && arena.Owns(second) && arena.Owns(third));
  assert(!pas::win::heap_realloc(heap, 0, recovered, capacity * 2));
  assert(pas::win::heap_allocations.contains(recovered) && recovered[0] == 73);
  assert(pas::win::heap_destroy(heap));
  assert(arena.LargestFree() == initial);
  // A saturated reserve must not need a second full copy to shrink an existing
  // buffer. Freeing its neighbour must permit growth into contiguous capacity.
  constexpr std::size_t mib = 1024u * 1024u;
  auto pressure_heap = pas::win::heap_create(0, 0, 0);
  auto* large = static_cast<unsigned char*>(pas::win::heap_alloc(pressure_heap, 0, 64 * mib));
  void* neighbour = pas::win::heap_alloc(pressure_heap, 0, 16 * mib);
  void* guard = pas::win::heap_alloc(pressure_heap, 0, 47 * mib);
  assert(large && neighbour && guard && arena.LargestFree() < mib);
  std::memset(large, 0x5b, 64 * mib);
  // Non-contiguous free capacity cannot satisfy growth; old ownership survives.
  assert(!pas::win::heap_realloc(pressure_heap, 8, large, 80 * mib));
  assert(pas::win::heap_allocations.contains(large));
  for (std::size_t i = 0; i < 64 * mib; ++i) assert(large[i] == 0x5b);
  auto* shrunk = static_cast<unsigned char*>(pas::win::heap_realloc(pressure_heap, 0, large, 32 * mib));
  std::printf("saturated reserve shrink result=%s largest_free=%zu\n", shrunk ? "allocated" : "null", arena.LargestFree());
  std::fflush(stdout);
  assert(shrunk); // No second 32 MiB buffer exists in this layout.
  for (std::size_t i = 0; i < 32 * mib; ++i) assert(shrunk[i] == 0x5b);
  assert(pas::win::heap_free(pressure_heap, 0, neighbour));
  auto* grown = static_cast<unsigned char*>(pas::win::heap_realloc(pressure_heap, 8, shrunk, 80 * mib));
  assert(grown == shrunk);
  for (std::size_t i = 0; i < 32 * mib; ++i) assert(grown[i] == 0x5b);
  for (std::size_t i = 32 * mib; i < 80 * mib; ++i) assert(grown[i] == 0);
  assert(pas::win::heap_destroy(pressure_heap));
  assert(arena.LargestFree() == initial && pas::win::heap_allocations.empty());
  // Split/coalesce repeated mixed-size allocations and arbitrary free order.
  for (unsigned round = 0; round < 100; ++round) {
    std::vector<void*> blocks;
    for (unsigned i = 0; i < 90; ++i) {
      void* block = arena.Allocate(65536 + ((i * 7919) % 1048576));
      assert(block && reinterpret_cast<std::uintptr_t>(block) % alignof(std::max_align_t) == 0);
      blocks.push_back(block);
    }
    for (unsigned i = 0; i < blocks.size(); i += 2) arena.Free(blocks[i]);
    for (unsigned i = 1; i < blocks.size(); i += 2) arena.Free(blocks[i]);
    assert(arena.LargestFree() == initial);
  }
  // Deterministic mixed realloc/free workload through the real heap API.
  // Check every retained byte, zero extension and failed-realloc ownership.
  struct Live { unsigned char* data{}; std::size_t size{}; unsigned char tag{}; };
  std::array<Live, 64> live{};
  std::uint32_t rng = 0x48c9ab17u;
  auto next = [&]() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
  auto stress_heap = pas::win::heap_create(0, 0, 0);
  unsigned moved = 0, in_place = 0, failed = 0;
  for (unsigned step = 0; step < 1024; ++step) {
    auto& item = live[next() % live.size()];
    for (std::size_t i = 0; i < item.size; ++i) assert(item.data[i] == item.tag);
    if (item.data && next() % 5 == 0) {
      assert(pas::win::heap_free(stress_heap, 0, item.data));
      item = {};
      continue;
    }
    const std::size_t bytes = step % 31 == 0 ? capacity * 2 :
        step % 29 == 0 ? 1 : 65536 + next() % (4 * mib);
    auto* changed = static_cast<unsigned char*>(item.data ?
        pas::win::heap_realloc(stress_heap, 8, item.data, bytes) :
        pas::win::heap_alloc(stress_heap, 8, bytes));
    if (!changed) {
      ++failed;
      if (item.data) {
        assert(pas::win::heap_allocations.at(item.data).size == item.size);
        for (std::size_t i = 0; i < item.size; ++i) assert(item.data[i] == item.tag);
      }
      continue;
    }
    assert(reinterpret_cast<std::uintptr_t>(changed) % alignof(std::max_align_t) == 0);
    if (item.data) { if (changed == item.data) ++in_place; else ++moved; }
    for (std::size_t i = 0; i < std::min(item.size, bytes); ++i) assert(changed[i] == item.tag);
    for (std::size_t i = item.size; i < bytes; ++i) assert(changed[i] == 0);
    item = {changed, bytes, static_cast<unsigned char>(step % 251 + 1)};
    std::memset(item.data, item.tag, item.size);
  }
  assert(moved && in_place && failed);
  assert(pas::win::heap_destroy(stress_heap));
  assert(pas::win::heap_allocations.empty() && arena.LargestFree() == initial);
  std::printf("native mixed realloc stress: operations=1024 moved=%u in_place=%u failure=%u coalesced=1\n",
      moved, in_place, failed);
  // The arena is shared by distinct native heaps, serialized by heap_mutex.
  // Destroying one owner must not invalidate buffers still used by another.
  std::array<std::thread, 4> workers;
  std::atomic<unsigned> ready{};
  std::atomic<bool> start{};
  for (unsigned owner = 0; owner < workers.size(); ++owner) {
    workers[owner] = std::thread([&, owner] {
      auto own_heap = pas::win::heap_create(0, 0, 0);
      ++ready;
      while (!start) std::this_thread::yield();
      for (unsigned round = 0; round < 128; ++round) {
        const unsigned char tag = static_cast<unsigned char>(owner * 37 + round % 31 + 1);
        auto* data = static_cast<unsigned char*>(pas::win::heap_alloc(own_heap, 0, 65536));
        assert(data && arena.Owns(data));
        std::memset(data, tag, 65536);
        auto* grown = static_cast<unsigned char*>(pas::win::heap_realloc(own_heap, 8, data, 131079));
        assert(grown);
        for (unsigned i = 0; i < 65536; ++i) assert(grown[i] == tag);
        for (unsigned i = 65536; i < 131079; ++i) assert(grown[i] == 0);
        auto* small = static_cast<unsigned char*>(pas::win::heap_realloc(own_heap, 0, grown, 32769));
        assert(small);
        for (unsigned i = 0; i < 32769; ++i) assert(small[i] == tag);
        assert(pas::win::heap_free(own_heap, 0, small));
      }
      assert(pas::win::heap_destroy(own_heap));
    });
  }
  while (ready != workers.size()) std::this_thread::yield();
  start = true;
  for (auto& worker : workers) worker.join();
  assert(pas::win::heap_allocations.empty() && arena.LargestFree() == initial);
  std::puts("native concurrent heaps: owners=4 rounds_per_owner=128 byte/zero-fill/cleanup/coalescing PASS");
  assert(!arena.Allocate(std::numeric_limits<std::size_t>::max()));
  deny_large = false;
  std::free(backing);
  std::puts("native heap reserve: hardware allocation sizes, failure ownership, zero fill, private destroy and coalescing PASS");
}
