// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif
#include "output_paths_backend.hpp"
#include <algorithm>
#include <codecvt>
#include <locale>
namespace srhd_awa::matrixgame::output_paths {
namespace fs=std::filesystem;
struct NativeRoot {std::wstring wide;fs::path path;};
namespace {
thread_local std::shared_ptr<const NativeRoot> active;
std::wstring Slashes(std::wstring path){std::replace(path.begin(),path.end(),L'\\',L'/');return path;}
std::string Utf8(std::wstring_view text){
    if(text.find(L'\0')!=text.npos)throw std::invalid_argument("NUL in output path");
    // Reject malformed Unicode rather than silently naming another file.
    for(std::size_t i=0;i<text.size();++i){const auto c=std::uint32_t(text[i]);
        if constexpr(sizeof(wchar_t)==2){if(c>=0xd800 && c<=0xdbff){if(++i==text.size() || std::uint32_t(text[i])<0xdc00 || std::uint32_t(text[i])>0xdfff)throw std::invalid_argument("unpaired output path surrogate");}else if(c>=0xdc00 && c<=0xdfff)throw std::invalid_argument("unpaired output path surrogate");}
        else if(c>0x10ffff || (c>=0xd800 && c<=0xdfff))throw std::invalid_argument("invalid output path scalar");
    }
    if constexpr(sizeof(wchar_t)==2)return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(text.data(),text.data()+text.size());
    else return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(text.data(),text.data()+text.size());
}
fs::path DecodePath(std::wstring_view text){return fs::u8path(Utf8(text));}
bool DeviceAbsolute(const std::wstring& path){
#ifdef __SWITCH__
    return path.starts_with(L"sdmc:/") && path.size()>6;
#else
    (void)path;return false;
#endif
}
void NoTraversal(const fs::path& path){for(const auto& part:path)if(part==".." || part==".")throw std::invalid_argument("output path traversal");}
fs::path NativePath(const std::wstring& input){
    if(!active)throw platform3g::Unsupported("native output scope unavailable");
    auto path=DecodePath(Slashes(input));NoTraversal(path);
    auto relative=path.lexically_relative(active->path);
    if(relative.empty() && path!=active->path)throw std::invalid_argument("output path outside native root");
    if(relative.is_absolute())throw std::invalid_argument("absolute output suffix");if(path!=active->path)NoTraversal(relative);
    // Existing symlinks must not redirect private outputs into other resources.
    fs::path current;
    for(const auto& part:path){current/=part;std::error_code ec;const auto status=fs::symlink_status(current,ec);
        if(ec && ec!=std::errc::no_such_file_or_directory)throw fs::filesystem_error("output path status",current,ec);
        if(fs::is_symlink(status))throw std::invalid_argument("symlink in native output path");}
    return path;
}
}
NativeScope::NativeScope(const wchar_t* root){
    if(active)throw std::logic_error("native output scope already bound");if(!root || !*root)throw std::invalid_argument("empty native output root");
    auto candidate=std::make_shared<NativeRoot>();candidate->wide=Slashes(root);candidate->path=DecodePath(candidate->wide);NoTraversal(candidate->path);
    if((!candidate->path.is_absolute() && !DeviceAbsolute(candidate->wide)) || candidate->path==candidate->path.root_path())throw std::invalid_argument("native output root must be an explicit private absolute directory");
    // Leave room for the original 260 wchar PersonalPath boundary and children.
    if(candidate->wide.size()>=240)throw std::length_error("native output root too long");
    while(candidate->wide.size()>1 && candidate->wide.back()==L'/')candidate->wide.pop_back();candidate->path=DecodePath(candidate->wide);
    root_=candidate;active=std::move(candidate);
    try{(void)NativePath(root_->wide);}catch(...){active.reset();root_.reset();throw;}
}
NativeScope::~NativeScope(){if(active!=root_)std::terminate();active.reset();}
bool NativeBound()noexcept{return bool(active);}
FolderItem::~FolderItem(){
#ifdef _WIN32
    if(native_)::CoTaskMemFree(native_);
#else
    if(native_)std::terminate();
#endif
}
std::int32_t PersonalLocation(FolderItem& out){
    if(out.native_ || out.root_)throw std::logic_error("personal folder item already owned");
    if(active){out.root_=active;return 0;}
#ifdef _WIN32
    ITEMIDLIST* item{};const auto result=::SHGetSpecialFolderLocation(nullptr,CSIDL_PERSONAL,&item);out.native_=item;return result;
#else
    throw platform3g::Unsupported("native output scope unavailable");
#endif
}
void PersonalPath(const FolderItem& item,wchar_t* path,std::size_t capacity){
    if(item.root_){if(!path || capacity<=item.root_->wide.size())throw std::invalid_argument("native output path buffer too short");std::copy(item.root_->wide.begin(),item.root_->wide.end(),path);path[item.root_->wide.size()]=0;return;}
#ifdef _WIN32
    static_assert(MAX_PATH==260);if(!item.native_ || !path || capacity<MAX_PATH)throw std::invalid_argument("invalid personal folder path buffer");
    if(!::SHGetPathFromIDListW(static_cast<ITEMIDLIST*>(item.native_),path))throw std::runtime_error("personal folder has no filesystem path");
#else
    throw platform3g::Unsupported("native output scope unavailable");
#endif
}
bool CreateFolder(const wchar_t* path){
    if(!path)throw std::invalid_argument("null output directory");
    if(active){auto target=NativePath(path);std::error_code ec;const bool created=fs::create_directories(target,ec);return !ec && created;}
#ifdef _WIN32
    return ::CreateDirectoryW(path,nullptr)!=0;
#else
    throw platform3g::Unsupported("native output scope unavailable");
#endif
}
std::wstring Finalize(std::wstring path){
    if(active){(void)NativePath(path);return Slashes(std::move(path));}
#ifdef _WIN32
    return path;
#else
    throw platform3g::Unsupported("native output scope unavailable");
#endif
}
fs::path FilesystemPath(const std::wstring& path){
    if(active)return NativePath(path);
#ifdef _WIN32
    return fs::path(path);
#else
    throw platform3g::Unsupported("native output scope unavailable");
#endif
}
}
