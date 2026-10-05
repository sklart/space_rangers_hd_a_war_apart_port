#include "package.hpp"
#include <cstdio>
#include <zlib.h>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  srhd_awa::package::Package package;
  std::string error;
  if (!package.Open(std::string(argv[1]) + "/DATA/common.pkg", &error)) return 1;
  const auto* entry = package.Resolve("DATA/Asteroid/00.gai");
  std::vector<uint8_t> payload;
  if (!entry || entry->kind != 2 || entry->data_size != 246863 ||
      !package.ReadPayload(*entry, &payload, &error) || payload.size() != 246863 ||
      crc32(0, payload.data(), static_cast<uInt>(payload.size())) != 0x045269e4u) return 1;
  std::puts("C++ payload regression passed");
  return 0;
}
