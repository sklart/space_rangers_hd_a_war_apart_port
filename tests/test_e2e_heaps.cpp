#include "windows.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <cstdlib>
#include <new>

static bool fail_new = false;
void* operator new(std::size_t bytes) {
  if (fail_new) throw std::bad_alloc();
  if (void* p = std::malloc(bytes ? bytes : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
  const auto process = pas::win::process_heap();
  const auto first = pas::win::heap_create(0, 16, 0);
  const auto second = pas::win::heap_create(1, 16, 0);
  assert(first != 0 && second != 0 && first != second && first != process);
  auto* bytes = static_cast<std::uint8_t*>(pas::win::heap_alloc(first, 8, 4));
  assert(bytes && bytes[0] == 0 && bytes[3] == 0);
  bytes[0] = 42;
  bool rejected = false;
  try { pas::win::heap_free(second, 0, bytes); }
  catch (const std::runtime_error&) { rejected = true; }
  assert(rejected);
  bytes = static_cast<std::uint8_t*>(pas::win::heap_realloc(first, 8, bytes, 8));
  assert(bytes && bytes[0] == 42 && bytes[7] == 0);
  assert(pas::win::heap_destroy(first));
  assert(!pas::win::heap_destroy(first));
  assert(pas::win::heap_destroy(second));
  auto* process_bytes = pas::win::heap_alloc(process, 0, 4);
  assert(process_bytes && pas::win::heap_free(process, 0, process_bytes));
  auto* growing = static_cast<std::uint8_t*>(pas::win::heap_alloc(process, 0, 8));
  growing[0] = 71;
  const auto node = reinterpret_cast<std::uintptr_t>(&pas::win::heap_allocations.at(growing));
  fail_new = true;
  // Tracking a successful realloc must not allocate, even if the payload moves.
  growing = static_cast<std::uint8_t*>(pas::win::heap_realloc(process, 8, growing, 4 * 1024 * 1024));
  fail_new = false;
  assert(growing && growing[0] == 71 && growing[4 * 1024 * 1024 - 1] == 0);
  assert(reinterpret_cast<std::uintptr_t>(&pas::win::heap_allocations.at(growing)) == node);
  assert(pas::win::heap_free(process, 0, growing));
  fail_new = true;
  bool caught = false;
  try { (void)pas::win::heap_alloc(process, 0, 1024); }
  catch (const std::bad_alloc&) { caught = true; }
  fail_new = false;
  assert(caught && pas::win::heap_allocations.empty());
  assert(!pas::win::heap_destroy(process));
}
