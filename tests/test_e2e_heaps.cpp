#include "windows.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>

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
  assert(!pas::win::heap_destroy(process));
}
