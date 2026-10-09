#include "units/SystemImports.hpp"

#include <cstdint>

int main() {
  if (SystemImports::ParamCount() != 0) return 1;

  std::uint8_t command[] = "Rangers \"saved game\"";
  pas::AnsiString token;
  auto* next = SystemImports::GetParamStr(command, token);
  if (token != "Rangers") return 2;
  SystemImports::GetParamStr(next, token);
  return token == "saved game" ? 0 : 3;
}
