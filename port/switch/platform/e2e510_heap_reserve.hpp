#ifndef SRHD_E2E510_HEAP_RESERVE_HPP
#define SRHD_E2E510_HEAP_RESERVE_HPP
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace srhd_awa::platform::heap_reserve {
// Called under the native heap mutex. Keep one contiguous reserve separate
// from ordinary small allocations; no reserve metadata allocates memory.
class Arena {
  struct alignas(std::max_align_t) Block {
    std::size_t size;
    Block* previous;
    Block* next;
    bool free;
  };
  Block* first_{};
  std::byte* end_{};
public:
  void Attach(void* memory, std::size_t bytes) {
    if (first_ || !memory || bytes < sizeof(Block) * 2) return;
    first_ = new(memory) Block{bytes - sizeof(Block), nullptr, nullptr, true};
    end_ = static_cast<std::byte*>(memory) + bytes;
  }
  bool Owns(const void* memory) const {
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    return first_ && address >= reinterpret_cast<std::uintptr_t>(first_ + 1) &&
           address < reinterpret_cast<std::uintptr_t>(end_);
  }
  void* Allocate(std::size_t bytes) {
    constexpr auto alignment = alignof(Block);
    if (bytes > std::numeric_limits<std::size_t>::max() - alignment) return nullptr;
    bytes = (std::max<std::size_t>(bytes, 1) + alignment - 1) & ~(alignment - 1);
    for (auto* block = first_; block; block = block->next) {
      if (!block->free || block->size < bytes) continue;
      if (block->size - bytes >= sizeof(Block) + alignment) {
        auto* tail = new(reinterpret_cast<std::byte*>(block + 1) + bytes)
            Block{block->size - bytes - sizeof(Block), block, block->next, true};
        if (tail->next) tail->next->previous = tail;
        block->next = tail;
        block->size = bytes;
      }
      block->free = false;
      return block + 1;
    }
    return nullptr;
  }
  void Free(void* memory) {
    if (!memory) return;
    auto* block = static_cast<Block*>(memory) - 1;
    block->free = true;
    if (block->next && block->next->free) {
      auto* next = block->next;
      block->size += sizeof(Block) + next->size;
      block->next = next->next;
      if (block->next) block->next->previous = block;
    }
    if (block->previous && block->previous->free) {
      auto* previous = block->previous;
      previous->size += sizeof(Block) + block->size;
      previous->next = block->next;
      if (previous->next) previous->next->previous = previous;
    }
  }
  std::size_t LargestFree() const {
    std::size_t result = 0;
    for (auto* block = first_; block; block = block->next)
      if (block->free) result = std::max(result, block->size);
    return result;
  }
};
inline Arena arena;
inline bool initialized{};
inline std::atomic<std::uint64_t> recoveries{};
inline std::atomic<std::size_t> reserved_bytes{};
inline void Initialize() {
  if (initialized) return;
  initialized = true;
#if defined(__SWITCH__)
  constexpr std::size_t bytes = 128u * 1024u * 1024u;
  // A failed reservation leaves the ordinary allocator behavior intact.
  void* backing = std::malloc(bytes);
  arena.Attach(backing, bytes);
  if (backing) reserved_bytes.store(bytes, std::memory_order_relaxed);
#endif
}
inline void* Allocate(std::size_t bytes) {
  if (void* memory = std::malloc(bytes)) return memory;
  // Small nodes must not fragment the emergency resource reserve.
  if (bytes < 64u * 1024u) return nullptr;
  void* memory = arena.Allocate(bytes);
  if (memory) ++recoveries;
  return memory;
}
inline void Free(void* memory) {
  if (arena.Owns(memory)) arena.Free(memory);
  else std::free(memory);
}
inline void* Reallocate(void* old, std::size_t bytes, std::size_t old_size) {
  if (!arena.Owns(old)) {
    if (void* memory = std::realloc(old, bytes)) return memory;
  }
  void* memory = Allocate(bytes);
  if (!memory) return nullptr; // The original allocation remains owned and valid.
  std::memcpy(memory, old, std::min(old_size, bytes));
  Free(old);
  return memory;
}
} // namespace srhd_awa::platform::heap_reserve
#endif
