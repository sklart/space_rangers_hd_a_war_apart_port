#include "package.hpp"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <zlib.h>

namespace {
std::atomic<std::size_t> live_bytes{0};
struct alignas(std::max_align_t) Allocation { std::size_t bytes; };
}
void* operator new(std::size_t bytes) {
  auto* allocation = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
  if (!allocation) throw std::bad_alloc();
  allocation->bytes = bytes;
  live_bytes += bytes;
  return allocation + 1;
}
void operator delete(void* p) noexcept {
  if (!p) return;
  auto* allocation = static_cast<Allocation*>(p) - 1;
  live_bytes -= allocation->bytes;
  std::free(allocation);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

int main(int argc, char** argv) {
  assert(argc == 2);
  srhd_awa::package::Package package;
  std::string error;
  assert(package.Open(std::string(argv[1]) + "/DATA/locations.pkg", &error));
  const auto baseline = live_bytes.load();
  std::uint32_t expected_crc = 0;
  for (int pass = 0; pass < 3; ++pass) {
    const auto handle = package.OpenEntryByPath("DATA/Gov/2FeiAnim1.gai", &error);
    assert(handle && package.GetEntrySize(handle) == 11296250);
    std::vector<std::uint8_t> data(11296250);
    assert(package.ReadEntry(handle, data.data(), data.size(), &error) == data.size());
    const auto crc = crc32(0, data.data(), data.size());
    if (pass == 0) expected_crc = crc;
    else assert(crc == expected_crc);
    const auto before_close = live_bytes.load();
    package.CloseEntry(handle);
    assert(before_close - live_bytes.load() >= data.size());
    assert(package.GetEntrySize(handle) == UINT64_MAX);
    data = {};
    std::vector<std::uint8_t>().swap(data);
    assert(live_bytes.load() < baseline + 8192);
  }
  std::printf("FeiAnim1 repeated read CRC=%08x; CloseEntry releases 11296250 bytes PASS\n", expected_crc);
}
