#include "gi_object.hpp"
#include "package.hpp"
#include "scene_compositor.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
using srhd_awa::package::Package;
using srhd_awa::platform::gi_object::GIObject;
using srhd_awa::platform::scene_compositor::Framebuffer;
using srhd_awa::platform::scene_compositor::Fingerprint;
using srhd_awa::platform::scene_compositor::Scene;

void Put32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) (*bytes)[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
std::vector<std::uint8_t> Gi(std::uint16_t color) {
  std::vector<std::uint8_t> bytes(104); std::memcpy(bytes.data(), "gi\0", 3);
  Put32(&bytes, 4, 1); Put32(&bytes, 16, 2); Put32(&bytes, 20, 2);
  Put32(&bytes, 24, 0xf800); Put32(&bytes, 28, 0x07e0); Put32(&bytes, 32, 0x001f);
  Put32(&bytes, 44, 1); Put32(&bytes, 64, 96); Put32(&bytes, 68, 8); Put32(&bytes, 80, 2); Put32(&bytes, 84, 2);
  for (std::size_t at = 96; at < 104; at += 2) { bytes[at] = static_cast<std::uint8_t>(color); bytes[at + 1] = static_cast<std::uint8_t>(color >> 8); }
  return bytes;
}
std::vector<std::uint8_t> Gai(std::uint16_t first, std::uint16_t second) {
  const auto frame0 = Gi(first), frame1 = Gi(second); constexpr std::size_t directory = 64;
  const auto frame1_at = directory + frame0.size(), table = frame1_at + frame1.size();
  std::vector<std::uint8_t> bytes(table + 36); std::memcpy(bytes.data(), "gai\0", 4);
  Put32(&bytes, 4, 1); Put32(&bytes, 16, 2); Put32(&bytes, 20, 2); Put32(&bytes, 24, 2);
  Put32(&bytes, 32, static_cast<std::uint32_t>(table)); Put32(&bytes, 36, 36);
  Put32(&bytes, 48, directory); Put32(&bytes, 52, static_cast<std::uint32_t>(frame0.size()));
  Put32(&bytes, 56, static_cast<std::uint32_t>(frame1_at)); Put32(&bytes, 60, static_cast<std::uint32_t>(frame1.size()));
  std::memcpy(bytes.data() + directory, frame0.data(), frame0.size()); std::memcpy(bytes.data() + frame1_at, frame1.data(), frame1.size());
  Put32(&bytes, table, 1); Put32(&bytes, table + 8, 16); Put32(&bytes, table + 16, 2);
  Put32(&bytes, table + 20, 0); Put32(&bytes, table + 24, 10); Put32(&bytes, table + 28, 1); Put32(&bytes, table + 32, 10);
  return bytes;
}
void Entry(std::vector<std::uint8_t>* bytes, std::size_t at, const char* name, std::int32_t kind, std::uint32_t target, std::uint32_t size) {
  std::memset(bytes->data() + at, 0, 158); Put32(bytes, at + 4, size); std::strncpy(reinterpret_cast<char*>(bytes->data() + at + 8), name, 62);
  std::strncpy(reinterpret_cast<char*>(bytes->data() + at + 71), name, 62); Put32(bytes, at + 134, static_cast<std::uint32_t>(kind)); Put32(bytes, at + 138, static_cast<std::uint32_t>(kind)); Put32(bytes, at + 150, target);
}
bool WritePackage(const char* path) {
  const auto a = Gai(0xf800, 0x07e0), b = Gai(0x001f, 0xffff);
  constexpr std::size_t root = 4, data = 174, payload_a = 502, data_a = payload_a + 4;
  const auto payload_b = data_a + a.size(), data_b = payload_b + 4;
  std::vector<std::uint8_t> bytes(data_b + b.size()); Put32(&bytes, 0, root);
  Put32(&bytes, root, 170); Put32(&bytes, root + 4, 1); Put32(&bytes, root + 8, 158); Entry(&bytes, root + 12, "DATA", 3, data, 0);
  Put32(&bytes, data, 328); Put32(&bytes, data + 4, 2); Put32(&bytes, data + 8, 158);
  Entry(&bytes, data + 12, "A.GAI", 0, static_cast<std::uint32_t>(payload_a), static_cast<std::uint32_t>(a.size()));
  Entry(&bytes, data + 170, "B.GAI", 0, static_cast<std::uint32_t>(payload_b), static_cast<std::uint32_t>(b.size()));
  std::memcpy(bytes.data() + data_a, a.data(), a.size()); std::memcpy(bytes.data() + data_b, b.data(), b.size());
  FILE* file = std::fopen(path, "wb"); if (!file) return false; const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(); std::fclose(file); return ok;
}
bool Same(const Fingerprint& left, const Fingerprint& right) { return left.crc32 == right.crc32 && left.fnv64 == right.fnv64 && left.canonical_bytes == right.canonical_bytes; }
}

int main() {
  constexpr const char* path = "build/test_gi_object.pkg";
  if (!WritePackage(path)) return 1;
  Package package; std::string error;
  GIObject a(&package), b(&package);
  const bool package_open = package.Open(path, &error);
  const auto package_summary = package_open ? package.Summarize() : srhd_awa::package::Summary{};
  const bool resolves_a = package_open && package.Resolve("DATA/A.gai") != nullptr;
  bool ok = package_open;
  a.SetId("object-a"); b.SetId("object-b");
  const bool a_loaded = package_open && a.LoadResource("DATA/A.gai", &error);
  ok = a_loaded && a.Metadata().frame_count == 2 && a.SourceFrame() == 0 && a.Image().pixels.size() == 16;
  if (!ok) { std::fprintf(stderr, "GI OBJECT FAIL: open=%d files=%u resolve=%d load A=%d frames=%d source=%d pixels=%zu (%s)\n", package_open, package_summary.files, resolves_a, a_loaded, a.Metadata().frame_count, a.SourceFrame(), a.Image().pixels.size(), error.c_str()); for (const auto& item : package_summary.paths) std::fprintf(stderr, "  %s\n", item.c_str()); return 1; }
  const auto first_pixels = a.Image().pixels;
  ok = ok && a.Update(10, &error) && a.SequenceFrame() == 1 && a.SourceFrame() == 1 && a.Image().pixels != first_pixels;
  ok = ok && b.LoadResource("DATA/B.gai", &error);
  if (!ok) { std::fprintf(stderr, "GI OBJECT FAIL: update/load B (%s)\n", error.c_str()); return 1; }
  a.SetPosition(0, 0); a.SetLayer(0); a.SetAlpha(128); b.SetPosition(1, 1); b.SetLayer(10);
  Scene scene; ok = ok && a.Draw(scene, &error) && b.Draw(scene, &error) && scene.SpriteCount() == 2;
  Fingerprint first{}; ok = ok && scene.ComputeFingerprint(&first, &error) && first.canonical_bytes > 0;
  std::vector<std::uint16_t> framebuffer(16, 0x001f); Framebuffer target{framebuffer.data(), 4, 4, 4};
  ok = ok && scene.Render(target, &error) && framebuffer[0] == 0x03ef;
  if (!ok) { std::fprintf(stderr, "GI OBJECT FAIL: alpha pixel=%04x (%s)\n", framebuffer[0], error.c_str()); return 1; }
  a.SetVisible(false); scene.Clear(); std::fill(framebuffer.begin(), framebuffer.end(), 0x001f);
  ok = ok && a.Draw(scene, &error) && scene.Render(target, &error) && framebuffer[0] == 0x001f;
  if (!ok) { std::fprintf(stderr, "GI OBJECT FAIL: visibility pixel=%04x (%s)\n", framebuffer[0], error.c_str()); return 1; }
  a.SetVisible(true); scene.Clear(); ok = ok && a.Update(0, &error) && a.Draw(scene, &error) && b.Draw(scene, &error);
  Fingerprint second{}; ok = ok && scene.ComputeFingerprint(&second, &error) && Same(first, second);
  std::remove(path); if (!ok) { std::fprintf(stderr, "GI OBJECT FAIL: determinism (%s)\n", error.c_str()); return 1; }
  std::puts("GI OBJECT PASS"); return 0;
}
