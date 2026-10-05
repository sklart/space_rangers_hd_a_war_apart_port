#include "ec_file_adapter.hpp"
#include "package.hpp"
#include "units/EC_File.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <zlib.h>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string package_path = std::string(argv[1]) + "/DATA/common.pkg";
  std::string error;
  srhd_awa::package::Package direct;
  if (!direct.Open(package_path, &error)) return 1;
  const auto* entry = direct.Resolve("DATA/Asteroid/00.gai");
  std::vector<uint8_t> expected;
  if (!entry || !direct.ReadPayload(*entry, &expected, &error)) return 1;
  if (!srhd_awa::platform::ec_file::OpenPackage(package_path, &error)) return 1;
  EC_File::TFileEC file{};
  EC_File::TFileEC_Create(&file);
  file.SetFileName(pas::WideString(u"data\\asteroid\\00.GAI"));
  if (!file.TryAcquireReadHandle(false) || file.GetSize() != expected.size()) return 1;
  std::vector<uint8_t> actual(expected.size());
  file.ReadBuffer(actual.data(), static_cast<uint32_t>(actual.size()));
  if (actual != expected || crc32(0, actual.data(), static_cast<uInt>(actual.size())) != 0x045269e4u) return 1;
  if (file.SetPointer(12, 0) != 12 || file.SetPointer(7, 1) != 19 || file.SetPointer(5, 2) != expected.size() - 5) return 1;
  uint8_t tail[5]{}; file.ReadBuffer(tail, sizeof(tail));
  if (std::memcmp(tail, expected.data() + expected.size() - sizeof(tail), sizeof(tail)) != 0) return 1;
  file.ReleaseHandle();
  file.SetFileName(pas::WideString(u"missing.bin"));
  if (file.TryAcquireReadHandle(false)) return 1;
  EC_File::TFileEC_Destroy(&file);
  srhd_awa::platform::ec_file::ClosePackage();
  std::puts("EC_File release regression passed");
  return 0;
}
