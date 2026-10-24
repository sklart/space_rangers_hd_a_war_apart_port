// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <cstddef>
namespace srhd_awa::matrixgame::output_paths {
class FolderItem {
public:
    FolderItem()=default;~FolderItem();
    FolderItem(const FolderItem&)=delete;
    FolderItem& operator=(const FolderItem&)=delete;
private:
    void* native_{};
    friend std::int32_t PersonalLocation(FolderItem&);
    friend void PersonalPath(const FolderItem&,wchar_t*,std::size_t);
};
std::int32_t PersonalLocation(FolderItem&);
void PersonalPath(const FolderItem&,wchar_t*,std::size_t);
bool CreateFolder(const wchar_t*);
}
