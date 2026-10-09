#include "win32_compat_heap.hpp"
#include "windows.hpp"
#include <cstdint>

namespace srhd_awa::platform::win32_compat {
namespace {
std::uint32_t HeapCreate(std::uint32_t flags, std::uint32_t initial,
                         std::uint32_t maximum) {
  return pas::win::heap_create(flags, initial, maximum);
}
std::int32_t HeapDestroy(std::uint32_t handle) {
  return pas::win::heap_destroy(handle) ? 1 : 0;
}
template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}

ImportAddress ResolveHeapImport(std::string_view dll, std::string_view symbol) {
  if (dll != "kernel32.dll") return nullptr;
  if (symbol == "HeapCreate") return Address(&HeapCreate);
  if (symbol == "HeapDestroy") return Address(&HeapDestroy);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
