#include "package.hpp"
#include <cstdio>

int main() {
  const char* path = "build/corrupt.pkg";
  FILE* file = std::fopen(path, "wb");
  if (!file) return 2;
  const unsigned char bytes[] = {4,0,0,0,170,0,0,0,1,0,0,0,158,0,0,0};
  std::fwrite(bytes, 1, sizeof(bytes), file);
  std::fclose(file);
  srhd_awa::package::Package package;
  std::string error;
  if (package.Open(path, &error)) return 1;
  std::printf("rejected=%s\n", error.c_str());
  std::remove(path);
  return 0;
}
