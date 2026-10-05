#include "package.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
struct Record { const char* name; int32_t kind; uint32_t target; uint32_t size{}; };
void u32(FILE* f, uint32_t n) { for (int i = 0; i != 4; ++i) std::fputc((n >> (8 * i)) & 255, f); }
void write_entry(FILE* f, const Record& r) { u32(f, 0); u32(f, r.size); char names[126]{}; std::strncpy(names, r.name, 62); std::strncpy(names + 63, r.name, 62); std::fwrite(names, 1, sizeof(names), f); u32(f, static_cast<uint32_t>(r.kind)); u32(f, static_cast<uint32_t>(r.kind)); u32(f, 0); u32(f, 0); u32(f, r.target); u32(f, 0); }
void write_folder(FILE* f, const std::vector<Record>& records) { u32(f, 170); u32(f, static_cast<uint32_t>(records.size())); u32(f, 158); for (const auto& r : records) write_entry(f, r); }
bool make(const char* path, const std::vector<std::vector<Record>>& folders, const char* payload = nullptr) { FILE* f = std::fopen(path, "wb"); if (!f) return false; u32(f, 4); for (const auto& records : folders) write_folder(f, records); if (payload) { u32(f, 0); std::fwrite(payload, 1, std::strlen(payload), f); } std::fclose(f); return true; }
bool fails_cycle(const char* path) { srhd_awa::package::Package p; std::string e; return !p.Open(path, &e) && e.find("cyclic folder reference") != std::string::npos; }
}

int main() {
  const char* path = "build/cycle.pkg";
  bool ok = true;
  auto check = [&](bool value, const char* label) { if (!value) std::fprintf(stderr, "failed: %s\n", label); return value; };
  ok &= check(make(path, {{{"A", 3, 174}}, {{"A", 3, 174}}}) && fails_cycle(path), "self cycle");
  ok &= check(make(path, {{{"A", 3, 174}}, {{"B", 3, 344}}, {{"A", 3, 174}}}) && fails_cycle(path), "indirect cycle");
  ok &= check(make(path, {{{"A", 3, 174}}, {{"B", 3, 344}}, {{"C", 3, 514}}, {{"FILE", 0, 684, 3}}}, "xyz"), "valid fixture write");
  { srhd_awa::package::Package p; std::string e; std::vector<uint8_t> payload; const srhd_awa::package::Entry* file = nullptr; ok &= check(p.Open(path, &e) && (file = p.Resolve("A/B/C/FILE")) && p.ReadPayload(*file, &payload, &e) && std::string(payload.begin(), payload.end()) == "xyz", "nested recursion and payload"); auto s = p.Summarize(); ok &= check(s.folders == 4 && s.files == 1 && s.max_depth == 3, "nested summary"); }
  ok &= check(make(path, {{{"A", 3, 332}, {"B", 3, 332}}, {{"FILE", 0, 502, 1}}}, "q"), "shared fixture write");
  { srhd_awa::package::Package p; std::string e; ok &= check(p.Open(path, &e) && p.Resolve("A/FILE") && p.Resolve("B/FILE"), "repeated completed offset"); }
  ok &= check(make(path, {{{"A", 3, 174}}, {{"A", 3, 174}}}) && fails_cycle(path), "cleanup failure setup");
  ok &= check(make(path, {{{"FILE", 0, 174, 1}}}, "z"), "cleanup valid rewrite");
  { srhd_awa::package::Package p; std::string e; ok &= check(p.Open(path, &e) && p.Resolve("FILE"), "cleanup after failure"); }
  std::remove(path); if (ok) std::puts("cycle suite passed"); return ok ? 0 : 1;
}
