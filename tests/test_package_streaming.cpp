#include "package.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <new>
#include <vector>

static bool bounded;
static size_t maximum;
#if defined(E2E511_PACKAGE_IO_PROFILE)
#include <sys/types.h>
#include <zlib.h>
static unsigned long long seeks, reads, decodes;
extern "C" int __real_fseeko(FILE*, off_t, int);
extern "C" size_t __real_fread(void*, size_t, size_t, FILE*);
extern "C" int __real_uncompress(Bytef*, uLongf*, const Bytef*, uLong);
extern "C" int __wrap_fseeko(FILE* f, off_t offset, int origin) {
  if (bounded) ++seeks;
  return __real_fseeko(f, offset, origin);
}
extern "C" size_t __wrap_fread(void* dst, size_t size, size_t count, FILE* f) {
  if (bounded) ++reads;
  return __real_fread(dst, size, count, f);
}
extern "C" int __wrap_uncompress(Bytef* dst, uLongf* size, const Bytef* src, uLong length) {
  if (bounded) ++decodes;
  return __real_uncompress(dst, size, src, length);
}
static void ResetIO() { seeks = reads = decodes = 0; }
static void ReportIO(const char* label) {
  std::printf("package io label=%s seeks=%llu reads=%llu decodes=%llu\n", label, seeks, reads, decodes);
}
#else
static void ResetIO() {}
static void ReportIO(const char*) {}
#endif

void* operator new(size_t size) {
  if (bounded) {
    maximum = std::max(maximum, size);
    if (size > 72112) throw std::bad_alloc();
  }
  if (void* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
  if (argc == 2) {
    srhd_awa::package::Package package;
    std::string error;
    assert(package.Open(argv[1], &error));
    unsigned checked = 0;
    std::vector<unsigned char> chunk(32771);
    for (const auto& row : package.Summarize().paths) {
      const auto path = row.substr(0, row.find('|'));
      const auto* entry = package.Resolve(path);
      if (!entry || entry->data_size < 1048576 ||
          (checked >= 8 && path.find("2Open.gai") == std::string::npos)) continue;
      std::vector<unsigned char> reference;
      assert(package.ReadPayload(*entry, &reference, &error));
      auto handle = package.OpenEntryByPath(path, &error);
      assert(handle);
      maximum = 0; ResetIO(); bounded = true;
      for (size_t offset = 0; offset < reference.size(); offset += chunk.size()) {
        const auto wanted = std::min(chunk.size(), reference.size() - offset);
        assert(package.ReadEntry(handle, chunk.data(), chunk.size(), &error) == wanted);
        assert(std::memcmp(chunk.data(), reference.data() + offset, wanted) == 0);
      }
      ReportIO("real-sequential");
      assert(package.SeekEntry(handle, 65533, &error));
      assert(package.ReadEntry(handle, chunk.data(), chunk.size(), &error) == chunk.size());
      assert(std::memcmp(chunk.data(), reference.data() + 65533, chunk.size()) == 0);
      bounded = false;
      package.CloseEntry(handle);
      ++checked;
      std::printf("real resource bytes=%zu largest_read_allocation=%zu equivalent=PASS path=%s\n",
                  reference.size(), maximum, path.c_str());
    }
    assert(checked > 0);
    return 0;
  }
  assert(argc == 5);
  srhd_awa::package::Package package;
  std::string error;
  assert(package.Open(argv[1], &error));
  auto handle = package.OpenEntryByPath("MUSIC", &error);
  assert(handle);
  std::vector<unsigned char> bytes(180003);
  ResetIO(); bounded = true;
  for (unsigned iteration = 0; iteration < 120; ++iteration) {
    const size_t position = (iteration * 31771u) % 1100003u;
    assert(package.SeekEntry(handle, position, &error));
    const auto wanted = std::min(bytes.size(), 1100003u - position);
    assert(package.ReadEntry(handle, bytes.data(), bytes.size(), &error) == wanted);
    for (size_t i = 0; i < wanted; ++i)
      assert(bytes[i] == ((position + i) * 17) % 251);
    assert(package.GetEntryPosition(handle) == position + wanted);
  }
  ReportIO("small-random");
  assert(maximum <= 72112);
  package.CloseEntry(handle);
  assert(package.GetEntryPosition(handle) == UINT64_MAX);
  auto a = package.OpenEntryByPath("MUSIC", &error);
  auto b = package.OpenEntryByPath("MUSIC", &error);
  assert(a && b && a != b);
  assert(package.SeekEntry(a, 65535, &error));
  assert(package.ReadEntry(a, bytes.data(), 3, &error) == 3);
  assert(package.GetEntryPosition(b) == 0);
  assert(package.ReadEntry(b, bytes.data(), 3, &error) == 3);
  assert(bytes[0] == 0 && bytes[1] == 17 && bytes[2] == 34);
  package.CloseEntry(a); package.CloseEntry(b);
  bounded = false;
  assert(package.Open(argv[2], &error));
  handle = package.OpenEntryByPath("MUSIC", &error);
  assert(handle);
  assert(package.SeekEntry(handle, 16 * 65536, &error));
  assert(package.ReadEntry(handle, bytes.data(), bytes.size(), &error) == 0);
  assert(error == "zlib block decode failed");
  assert(package.GetEntryPosition(handle) == 16 * 65536);
  // A later healthy block remains readable after the malformed final block.
  assert(package.SeekEntry(handle, 0, &error));
  assert(package.ReadEntry(handle, bytes.data(), 3, &error) == 3);
  package.CloseEntry(handle);
  // Uncompressed entries share the tracked FILE position path.
  assert(package.Open(argv[4], &error));
  handle = package.OpenEntryByPath("MUSIC", &error);
  ResetIO(); bounded = true;
  for (size_t position = 0; position < 1100003; position += bytes.size()) {
    const size_t wanted = std::min<size_t>(bytes.size(), 1100003 - position);
    assert(package.ReadEntry(handle, bytes.data(), bytes.size(), &error) == wanted);
    for (size_t i = 0; i < wanted; ++i) assert(bytes[i] == ((position + i) * 17) % 251);
  }
  ReportIO("raw-sequential");
  assert(package.ReadEntry(handle, bytes.data(), 1, &error) == 0);
  assert(package.SeekEntry(handle, 1100003 - 17, &error));
  assert(package.ReadEntry(handle, bytes.data(), 100, &error) == 17);
  assert(bytes[0] == ((1100003 - 17) * 17) % 251);
  assert(package.SeekEntry(handle, 0, &error));
  assert(package.ReadEntry(handle, bytes.data(), 3, &error) == 3);
  assert(bytes[0] == 0 && bytes[1] == 17 && bytes[2] == 34);
  package.CloseEntry(handle); bounded = false;
  // Exercise more blocks than the fixed lookup cache can store individually.
  bounded = false;
  assert(package.Open(argv[3], &error));
  const auto* entry = package.Resolve("MUSIC");
  assert(entry && entry->data_size > 256u * 65536);
  std::vector<unsigned char> reference;
  assert(package.ReadPayload(*entry, &reference, &error));
  handle = package.OpenEntryByPath("MUSIC", &error);
  maximum = 0; ResetIO(); bounded = true;
  for (unsigned iteration = 0; iteration < 180; ++iteration) {
    const size_t position = (uint64_t(iteration) * 1047293u) % reference.size();
    const size_t wanted = std::min(bytes.size(), reference.size() - position);
    assert(package.SeekEntry(handle, position, &error));
    assert(package.ReadEntry(handle, bytes.data(), bytes.size(), &error) == wanted);
    assert(std::memcmp(bytes.data(), reference.data() + position, wanted) == 0);
  }
  ReportIO("large-random");
  assert(maximum <= 72112);
  package.CloseEntry(handle);
  bounded = false;
  // A short read must not leave a stale cached FILE position or sticky EOF.
  // Restore the same private fixture and retry on the same open reader.
  const std::filesystem::path retry_path = std::string(argv[1]) + ".retry";
  std::filesystem::copy_file(argv[1], retry_path, std::filesystem::copy_options::overwrite_existing);
  std::ifstream source(argv[1], std::ios::binary);
  std::vector<unsigned char> disk((std::istreambuf_iterator<char>(source)), {});
  size_t late_header = 178;
  for (unsigned i = 0; i < 16; ++i) {
    const auto* p = disk.data() + late_header;
    const uint32_t size = uint32_t(p[0]) | uint32_t(p[1]) << 8 |
                          uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    late_header += 4 + size;
  }
  assert(package.Open(retry_path.string(), &error));
  handle = package.OpenEntryByPath("MUSIC", &error);
  std::filesystem::resize_file(retry_path, late_header);
  assert(package.SeekEntry(handle, 16 * 65536, &error));
  assert(package.ReadEntry(handle, bytes.data(), bytes.size(), &error) == 0);
  assert(package.GetEntryPosition(handle) == 16 * 65536);
  {
    std::ofstream restore(retry_path, std::ios::binary | std::ios::trunc);
    restore.write(reinterpret_cast<const char*>(disk.data()), disk.size());
    assert(restore.good());
  }
  const auto tail = 1100003 - 16 * 65536;
  assert(package.ReadEntry(handle, bytes.data(), bytes.size(), &error) == tail);
  for (size_t i = 0; i < tail; ++i) assert(bytes[i] == ((16 * 65536 + i) * 17) % 251);
  package.CloseEntry(handle);
  std::printf("bounded package streaming: random seeks, interleaved readers, corrupt retry PASS; largest allocation=%zu\n", maximum);
}
