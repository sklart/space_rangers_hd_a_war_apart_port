#include "e2e510_heap_reserve.hpp"
#include "windows.hpp"
#include <cassert>
#include <cstdio>
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
  assert(!arena.Allocate(std::numeric_limits<std::size_t>::max()));
  deny_large = false;
  std::free(backing);
  std::puts("native heap reserve: hardware allocation sizes, failure ownership, zero fill, private destroy and coalescing PASS");
}
