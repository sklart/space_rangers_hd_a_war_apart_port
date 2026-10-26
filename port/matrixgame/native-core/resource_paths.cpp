// SPDX-License-Identifier: GPL-2.0-or-later
#include "resource_paths.hpp"
#include <algorithm>
#include <codecvt>
#include <locale>
#include <mutex>
#include <stdexcept>
namespace srhd_awa::matrixgame::resource_paths {
namespace fs=std::filesystem;
struct Root {fs::path path;};
namespace {
struct Registry {std::mutex mutex;std::shared_ptr<const Root> active;};
Registry& State(){static Registry state;return state;}
bool Rooted(const fs::path& path){
 if(path.is_absolute())return true;
 // newlib devoptab paths (sdmc:/, romfs:/, or a host-provided mount) are rooted
 // even though std::filesystem's POSIX grammar calls them relative.
 const auto text=path.generic_u8string();const auto colon=text.find(':');
 return colon!=text.npos && colon>0 && colon+1<text.size() && text[colon+1]=='/' && text.find('/')>colon;
}
fs::path Decode(const wchar_t* input){
 if(!input || !*input)throw std::invalid_argument("empty native resource root");
 std::wstring text(input);std::replace(text.begin(),text.end(),L'\\',L'/');
#ifdef _WIN32
 return fs::path(text);
#else
 if constexpr(sizeof(wchar_t)==2)return fs::u8path(std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(text));
 else return fs::u8path(std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(text));
#endif
}
}
NativeScope::NativeScope(const wchar_t* input){
 auto& state=State();
 {std::lock_guard lock(state.mutex);if(state.active)throw std::logic_error("native resource root already bound");}
 auto root=std::make_shared<Root>();root->path=Decode(input).lexically_normal();
 if(!Rooted(root->path))throw std::invalid_argument("native resource root must be absolute");
 std::error_code error;if(!fs::is_directory(root->path,error) || error)throw std::invalid_argument("native resource root directory unavailable");
 std::lock_guard lock(state.mutex);if(state.active)throw std::logic_error("native resource root already bound");
 root_=std::move(root);state.active=root_;
}
NativeScope::~NativeScope(){auto& state=State();std::lock_guard lock(state.mutex);if(state.active!=root_)std::terminate();state.active.reset();}
bool Bound()noexcept{auto& state=State();std::lock_guard lock(state.mutex);return bool(state.active);}
fs::path PlanReadPath(const fs::path& root,const fs::path& name){return Rooted(name)?name:root/name;}
fs::path ReadDiskPath(const fs::path& name){
 auto& state=State();std::shared_ptr<const Root> root;
 {std::lock_guard lock(state.mutex);root=state.active;}
 return root?PlanReadPath(root->path,name):name;
}
}
