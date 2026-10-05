#include "filesystem.hpp"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: test_package_probe <game-root>\n");
    return 2;
  }
  srhd_awa::platform::PackageRootInfo info{};
  char error[128]{};
  if (!srhd_awa::platform::ProbePackageRoot(argv[1], "DATA/common.pkg", &info, error, sizeof(error))) {
    std::fprintf(stderr, "probe failed: %s\n", error);
    return 1;
  }
  if (info.root_offset != 4 || info.header_size != 170 || info.entry_count != 1 ||
      info.entry_record_size != 158 || std::strcmp(info.first_entry_name, "DATA") != 0) {
    std::fprintf(stderr, "unexpected release package metadata\n");
    return 1;
  }
  std::puts("package probe passed");
  return 0;
}
