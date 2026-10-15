#include "units/SysUtilsImports.hpp"
#include "units/SysUtils.hpp"
#include "e2e_file_search.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

int main(int argc, char** argv) {
  namespace search = srhd_awa::platform::e2e_file_search;
  assert(argc == 2);
  const std::filesystem::path fixture(argv[1]);
  std::filesystem::create_directories(fixture);
  std::ofstream(fixture / "INSTALL_english.txt") << "english";
  std::ofstream(fixture / "install_RUSSIAN.TXT") << "russian";
  std::ofstream(fixture / "README") << "other";
  const auto previous = std::filesystem::current_path();
  std::filesystem::current_path(fixture);

  SysUtils::TSearchRec found{};
  std::set<std::string> names;
  auto result = search::First("INSTALL_*.txt", SysUtilsImports::faAnyFile, found);
  assert(result == 0);
  do {
    names.insert(found.Name.c_str());
    assert(found.Size > 0);
    result = search::Next(found);
  } while (result == 0);
  assert(names == std::set<std::string>({"INSTALL_english.txt", "install_RUSSIAN.TXT"}));
  search::Close(found);
  assert(found.FindHandle == 0xffffffffu);

  assert(search::First("MISSING_*.txt", SysUtilsImports::faAnyFile, found) != 0);
  assert(search::First("*.*", SysUtilsImports::faAnyFile, found) == 0);
  search::Close(found);

  std::filesystem::current_path(previous);
  std::filesystem::remove(fixture / "INSTALL_english.txt");
  std::filesystem::remove(fixture / "install_RUSSIAN.TXT");
  std::filesystem::remove(fixture / "README");
  std::filesystem::remove(fixture);
}
