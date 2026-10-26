// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
namespace srhd_awa::matrixgame::output_paths {
struct NativeRoot;
// Explicit process/thread binding. The caller chooses a private output root;
// no native fallback into current game resources or the host's saved games.
class NativeScope {
public:
    explicit NativeScope(const wchar_t* root);
    ~NativeScope();
    NativeScope(const NativeScope&)=delete;
    NativeScope& operator=(const NativeScope&)=delete;
private:
    std::shared_ptr<const NativeRoot> root_;
};
class FolderItem {
public:
    FolderItem()=default;~FolderItem();
    FolderItem(const FolderItem&)=delete;
    FolderItem& operator=(const FolderItem&)=delete;
private:
    void* native_{};
    std::shared_ptr<const NativeRoot> root_;
    friend std::int32_t PersonalLocation(FolderItem&);
    friend void PersonalPath(const FolderItem&,wchar_t*,std::size_t);
};
std::int32_t PersonalLocation(FolderItem&);
void PersonalPath(const FolderItem&,wchar_t*,std::size_t);
bool CreateFolder(const wchar_t*);
// Preserve original Windows output when no native scope is bound. Native
// paths keep the original SpaceRangersHD/Cache/Screenshots layout with '/'.
std::wstring Finalize(std::wstring);
std::filesystem::path FilesystemPath(const std::wstring&);
bool NativeBound()noexcept;
}
