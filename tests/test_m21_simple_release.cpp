#include "image_cpu.hpp"
#include "package.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <zlib.h>

namespace {
constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ull;

std::uint64_t Fnv(const std::uint8_t* bytes, std::size_t size) {
  std::uint64_t value = kFnvOffset;
  for (std::size_t i = 0; i < size; ++i) value = (value ^ bytes[i]) * kFnvPrime;
  return value;
}

bool Expect(bool value, const char* what) {
  if (!value) std::fprintf(stderr, "FAIL: %s\n", what);
  return value;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  srhd_awa::package::Package package;
  std::string error;
  if (!Expect(package.Open(argv[1], &error), error.c_str())) return 1;
  const auto* entry = package.Resolve("DATA/Planet/Spu00.png");
  std::vector<std::uint8_t> source;
  if (!Expect(entry != nullptr && package.ReadPayload(*entry, &source, &error), "read configured Simple source")) return 1;
  srhd_awa::platform::image_cpu::Image image;
  if (!Expect(srhd_awa::platform::image_cpu::Decode(source.data(), static_cast<std::int32_t>(source.size()),
      srhd_awa::platform::image_cpu::Format::RGB565, &image, &error), error.c_str())) return 1;
  const auto source_crc = crc32(0, source.data(), static_cast<uInt>(source.size()));
  const auto decoded_crc = crc32(0, image.pixels.data(), static_cast<uInt>(image.pixels.size()));
  const bool exact = source.size() == 7389 && source_crc == 0xa3721a9cu && Fnv(source.data(), source.size()) == 0x32ebfdd05d7fa674ull &&
      image.width == 128 && image.height == 60 && image.pitch == 256 && image.pixels.size() == 15360 &&
      decoded_crc == 0x51e16db2u && Fnv(image.pixels.data(), image.pixels.size()) == 0xffeaf550d3c28655ull;
  std::printf("M21 SIMPLE RELEASE resource=DATA/Planet/Spu00.png source_size=%zu source_crc32=%08x source_fnv64=%016llx width=%d height=%d pitch=%d decoded_crc32=%08x decoded_fnv64=%016llx\n",
      source.size(), source_crc, static_cast<unsigned long long>(Fnv(source.data(), source.size())), image.width, image.height,
      image.pitch, decoded_crc, static_cast<unsigned long long>(Fnv(image.pixels.data(), image.pixels.size())));
  return Expect(exact, "independent Python oracle fingerprint") ? 0 : 1;
}
