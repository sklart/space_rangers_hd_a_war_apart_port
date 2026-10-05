#pragma once
#include <string>
#include <vector>

namespace srhd_awa::platform::ec_file {
bool OpenPackage(const std::string& package_path, std::string* error);
bool OpenPackages(const std::vector<std::string>& package_paths, std::string* error);
void ClosePackage();
}
