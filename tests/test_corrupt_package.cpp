#include "package.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
void u32(FILE* f, uint32_t n) { for (int i = 0; i != 4; ++i) std::fputc((n >> (8 * i)) & 255, f); }
void entry(FILE* f, const char* name, int32_t kind, uint32_t target, uint32_t size = 0) {
  u32(f, 0); u32(f, size); char names[126]{}; std::strncpy(names, name, 62); std::strncpy(names + 63, name, 62);
  std::fwrite(names, 1, sizeof(names), f); u32(f, static_cast<uint32_t>(kind)); u32(f, static_cast<uint32_t>(kind));
  u32(f, 0); u32(f, 0); u32(f, target); u32(f, 0);
}
void folder(FILE* f, uint32_t count, uint32_t record = 158, uint32_t header = 170) { u32(f, header); u32(f, count); u32(f, record); }
bool rejected(const char* path) { srhd_awa::package::Package p; std::string e; return !p.Open(path, &e); }
bool payload_rejected(const char* path) { srhd_awa::package::Package p; std::string e; std::vector<uint8_t> out; return p.Open(path, &e) && p.Resolve("FILE") && !p.ReadPayload(*p.Resolve("FILE"), &out, &e); }
bool write_bytes(const char* path, const std::vector<unsigned char>& b) { FILE* f = std::fopen(path, "wb"); if (!f) return false; bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size(); std::fclose(f); return ok; }
}

int main() {
  const char* path = "build/corrupt.pkg";
  auto check = [&](bool value, const char* label) { if (!value) std::fprintf(stderr, "failed: %s\n", label); return value; };
  bool ok = true;
  ok &= check(write_bytes(path, {4, 0, 0}), "short root pointer") && check(rejected(path), "short root rejection");
  ok &= check(write_bytes(path, {0xff, 0xff, 0xff, 0x7f}), "root eof") && check(rejected(path), "root eof rejection");
  auto write = [&](const auto& emit) { FILE* f = std::fopen(path, "wb"); if (!f) return false; emit(f); std::fclose(f); return true; };
  ok &= check(write([&](FILE* f) { u32(f, 4); u32(f, 170); }), "truncated header write") && check(rejected(path), "truncated header");
  for (const auto& c : std::vector<std::pair<uint32_t, uint32_t>>{{11, 158}, {170, 157}, {0xffffffffu, 158}, {0, 158}}) {
    ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 0, c.second, c.first); }), "invalid header write") && check(rejected(path), "invalid header fields");
  }
  ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 0xffffffffu); }), "huge count write") && check(rejected(path), "huge count");
  ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 1); }), "truncated entry write") && check(rejected(path), "truncated entry");
  ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 1); entry(f, "A", 3, 0xfffffff0u); }), "child eof write") && check(rejected(path), "child eof");
  ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 1); entry(f, "A", 3, 4); }), "self cycle write") && check(rejected(path), "self cycle");
  ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 1); entry(f, "A", 3, 174); folder(f, 1); entry(f, "B", 3, 4); }), "indirect cycle write") && check(rejected(path), "indirect cycle");
  for (int variant = 0; variant != 8; ++variant) {
    ok &= check(write([&](FILE* f) { u32(f, 4); folder(f, 1); entry(f, "FILE", 2, 174, variant == 7 ? 0xffffffffu : 1); u32(f, 0);
      if (variant == 1) { u32(f, 7); }
      if (variant == 2) { u32(f, 80000); }
      if (variant == 3) { u32(f, 8); std::fwrite("NOPE", 1, 4, f); u32(f, 1); }
      if (variant == 4) { u32(f, 8); std::fwrite("ZL02", 1, 4, f); u32(f, 2); }
      if (variant == 5) { u32(f, 8); std::fwrite("ZL02", 1, 4, f); u32(f, 1); }
      if (variant == 6) { u32(f, 10); std::fwrite("ZL02", 1, 4, f); u32(f, 1); std::fputc(0, f); std::fputc(0, f); } }), "compressed write") && check(payload_rejected(path), "compressed corruption");
  }
  std::remove(path); if (ok) std::puts("corrupt package suite passed"); return ok ? 0 : 1;
}
