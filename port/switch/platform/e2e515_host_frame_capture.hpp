#pragma once
// Opt-in host diagnostics. Never compiled into the Switch presentation path.
#include <cstdio>
#include <cstdint>
#include <vector>

namespace srhd_awa::platform::host_frame_capture {
inline void Requested(const char* directory, unsigned screen, const void* pixels,
                      int pitch, int width, int height, unsigned depth, bool registered_owner,
                      int aux_width, int aux_height, std::uint64_t hash) {
  static unsigned completed{};
  char path[1200]{};
  std::snprintf(path, sizeof(path), "%s/frame-request.txt", directory);
  unsigned sequence{}, expected{};
  if (auto* input = std::fopen(path, "r")) {
    const auto count = std::fscanf(input, "%u %u", &sequence, &expected);
    std::fclose(input);
    if (count != 2) return;
  } else return;
  if (sequence <= completed || sequence > 256 || expected != screen) return;
  std::snprintf(path, sizeof(path), "%s/frame-%u.ppm", directory, sequence);
  bool ok = false;
  if (auto* output = std::fopen(path, "wb")) {
    ok = std::fprintf(output, "P6\n%d %d\n255\n", width, height) > 0;
    std::vector<unsigned char> row(static_cast<std::size_t>(width) * 3);
    for (int y = 0; y < height && ok; ++y) {
      const auto* source = reinterpret_cast<const std::uint16_t*>(
          static_cast<const unsigned char*>(pixels) + y * pitch);
      for (int x = 0; x < width; ++x) {
        row[x * 3] = ((source[x] >> 11) & 31) * 255 / 31;
        row[x * 3 + 1] = ((source[x] >> 5) & 63) * 255 / 63;
        row[x * 3 + 2] = (source[x] & 31) * 255 / 31;
      }
      ok = std::fwrite(row.data(), 1, row.size(), output) == row.size();
    }
    if (std::fclose(output)) ok = false;
  }
  char temporary[1200]{};
  std::snprintf(temporary, sizeof(temporary), "%s/frame-%u.json.tmp", directory, sequence);
  if (auto* output = std::fopen(temporary, "w")) {
    const bool written = std::fprintf(output,
        "{\"sequence\":%u,\"screen\":%u,\"width\":%d,\"height\":%d,\"pitch\":%d,"
        "\"depth\":%u,\"registered_owner\":%s,\"reason\":\"explicit-host-request\",\"aux_width\":%d,\"aux_height\":%d,\"rgb565_hash\":\"%016llx\",\"complete\":%s}\n",
        sequence, screen, width, height, pitch, depth, registered_owner ? "true" : "false", aux_width, aux_height,
        static_cast<unsigned long long>(hash), ok ? "true" : "false") > 0;
    const bool closed = std::fclose(output) == 0;
    std::snprintf(path, sizeof(path), "%s/frame-%u.json", directory, sequence);
    if (written && closed && std::rename(temporary, path) == 0) completed = sequence;
  }
}
}
