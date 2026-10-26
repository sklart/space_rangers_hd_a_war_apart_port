// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <filesystem>
#include <memory>
namespace srhd_awa::matrixgame::resource_paths {
struct Root;
// Single original-engine read root, visible to its asset worker threads. Does
// not change process cwd, package keys, write paths or release file owners.
class NativeScope {
 std::shared_ptr<const Root> root_;
public:
 explicit NativeScope(const wchar_t*);
 ~NativeScope();
 NativeScope(const NativeScope&)=delete;
 NativeScope& operator=(const NativeScope&)=delete;
};
bool Bound()noexcept;
std::filesystem::path PlanReadPath(const std::filesystem::path& root,const std::filesystem::path& name);
std::filesystem::path ReadDiskPath(const std::filesystem::path& name);
}
