#include "units/SystemImports.hpp"

#include "win32_compat.hpp"

#include <cstdint>
#include <string_view>

#if defined(E2E_HOST_OKGF_STUB)
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveOkgfImport(std::string_view) { return nullptr; }
}
#endif

int main() {
  if (SystemImports::ParamCount() != 0) return 1;

  std::uint8_t command[] = "Rangers \"saved game\"";
  pas::AnsiString token;
  auto* next = SystemImports::GetParamStr(command, token);
  if (token != "Rangers") return 2;
  SystemImports::GetParamStr(next, token);
  return token == "saved game" ? 0 : 3;
}
