// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif
#include "output_paths_backend.hpp"
namespace srhd_awa::matrixgame::output_paths {
#ifdef _WIN32
FolderItem::~FolderItem(){if(native_)::CoTaskMemFree(native_);}
std::int32_t PersonalLocation(FolderItem& out){
    if(out.native_)throw std::logic_error("personal folder item already owned");
    ITEMIDLIST* item{};
    const auto result=::SHGetSpecialFolderLocation(nullptr,CSIDL_PERSONAL,&item);
    out.native_=item;
    return result;
}
void PersonalPath(const FolderItem& item,wchar_t* path,std::size_t capacity){
    static_assert(MAX_PATH==260);
    if(!item.native_ || !path || capacity<MAX_PATH)throw std::invalid_argument("invalid personal folder path buffer");
    if(!::SHGetPathFromIDListW(static_cast<ITEMIDLIST*>(item.native_),path))
        throw std::runtime_error("personal folder has no filesystem path");
}
bool CreateFolder(const wchar_t* path){return ::CreateDirectoryW(path,nullptr)!=0;}
#else
FolderItem::~FolderItem(){if(native_)std::terminate();}
std::int32_t PersonalLocation(FolderItem&){throw platform3g::Unsupported("native output personal-folder mapping unavailable");}
void PersonalPath(const FolderItem&,wchar_t*,std::size_t){throw platform3g::Unsupported("native output personal-folder mapping unavailable");}
bool CreateFolder(const wchar_t*){throw platform3g::Unsupported("native output directory creation unavailable");}
#endif
}
