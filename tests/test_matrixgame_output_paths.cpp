// SPDX-License-Identifier: GPL-2.0-or-later
#include "output_paths_backend.hpp"
#include "MatrixGamePathUtils.hpp"
#include "CFile.hpp"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <locale>
#include <codecvt>
namespace op=srhd_awa::matrixgame::output_paths;
namespace fs=std::filesystem;
template<class F>void RejectAt(F f,int line){bool refused=false;try{f();}catch(const std::exception&){refused=true;}if(!refused){std::fprintf(stderr,"Expected refusal at line %d\n",line);std::abort();}}
#define Reject(...) RejectAt(__VA_ARGS__,__LINE__)
std::wstring Wide(const std::string& s){
 if constexpr(sizeof(wchar_t)==2)return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(s);
 else return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(s);
}
int main(int argc,char** argv){assert(argc==2);assert(!op::NativeBound());auto base=Wide(argv[1]);std::replace(base.begin(),base.end(),L'\\',L'/');auto root=base+L"/native-\u041a\u044d\u0448-\U0001f680";auto basePath=fs::u8path(argv[1]);assert(basePath.is_absolute() && !fs::exists(basePath));
 Reject([]{op::NativeScope scope(nullptr);});Reject([]{op::NativeScope scope(L".");});Reject([]{op::NativeScope scope(L"relative/cache");});Reject([]{op::NativeScope scope(L"/");});Reject([]{op::NativeScope scope(L"/tmp/../elsewhere");});assert(!op::NativeBound());
 std::wstring invalid=L"/tmp/bad-";invalid.push_back(wchar_t(0xd800));Reject([&]{op::NativeScope scope(invalid.c_str());});assert(!op::NativeBound());
 op::FolderItem retained;
 {
  op::NativeScope scope(root.c_str());assert(op::NativeBound());assert(!fs::exists(basePath));Reject([&]{op::NativeScope second(root.c_str());});assert(op::NativeBound());
  assert(op::PersonalLocation(retained)==0);Reject([&]{op::PersonalLocation(retained);});wchar_t buffer[260]{};op::PersonalPath(retained,buffer,260);assert(std::wstring(buffer)==root);Reject([&]{op::PersonalPath(retained,buffer,3);});
  const auto cache=PathToOutputFiles(L"Cache");assert(cache==root+L"/SpaceRangersHD/Cache" && cache.find(L'\\')==cache.npos);assert(fs::is_directory(op::FilesystemPath(root+L"/SpaceRangersHD")));assert(!fs::exists(op::FilesystemPath(cache)));
  assert(!op::CreateFolder((root+L"/SpaceRangersHD").c_str())); // Original Windows CreateDirectory false when already present.
  auto screens=PathToOutputFiles(L"Screenshots");assert(screens==root+L"/SpaceRangersHD/Screenshots");
  const auto cachePath=op::FilesystemPath(cache);assert(fs::create_directory(cachePath));
  auto filename=cache+L"/map-\u043a\u0430\u0440\u0442\u0430-\U0001f680.1234";
  // Actual original CFile backend round-trip, using the exact final wide path.
  Base::CFile file(filename);file.Create();unsigned payload[4]{0x12345678,0xabcdef01,0,0xfedcba98};file.Write(payload,sizeof(payload));file.Close();file.OpenRead();unsigned result[4]{};file.Read(result,sizeof(result));file.Close();for(unsigned i=0;i<4;++i)assert(result[i]==payload[i]);assert(fs::file_size(op::FilesystemPath(filename))==sizeof(payload));
  assert(op::Finalize(root+L"\\SpaceRangersHD\\Cache")==cache);Reject([&]{op::FilesystemPath(base+L"/outside");});Reject([&]{op::CreateFolder((root+L"/../escape").c_str());});Reject([&]{PathToOutputFiles(L"../../escape");});assert(!fs::exists(basePath/"escape"));
  auto nul=cache; nul.push_back(0);nul+=L"hidden";Reject([&]{op::FilesystemPath(nul);});Reject([&]{op::Finalize(nul);});
  auto filePath=op::FilesystemPath(filename);bool failed=false;try{failed=!op::CreateFolder((filename+L"/child").c_str());}catch(const std::exception&){failed=true;}assert(failed);assert(fs::file_size(filePath)==sizeof(payload));
#ifndef _WIN32
  const auto destination=basePath/"outside";fs::create_directory(destination);auto link=op::FilesystemPath(root+L"/link");fs::create_directory_symlink(destination,link);Reject([&]{op::FilesystemPath(root+L"/link/cache");});assert(!fs::exists(destination/"cache"));
#endif
 }
 assert(!op::NativeBound());wchar_t path[260]{};op::PersonalPath(retained,path,260);assert(std::wstring(path)==root); // Owned folder snapshot survives unbinding.
#ifndef _WIN32
 Reject([]{op::Finalize(L"./Cache");});op::FolderItem missing;Reject([&]{op::PersonalLocation(missing);});Reject([&]{op::CreateFolder(root.c_str());});
#else
 auto original=op::Finalize(L"C:\\original\\Cache");assert(original==L"C:\\original\\Cache");assert(op::FilesystemPath(original)==fs::path(original));
#endif
 // Exceptional unwinding releases binding and permits a new independent session.
 try{op::NativeScope scope((base+L"/second").c_str());throw std::bad_alloc();}catch(const std::bad_alloc&){}assert(!op::NativeBound());{op::NativeScope scope((base+L"/third").c_str());assert(op::NativeBound());}assert(!op::NativeBound());
 std::puts("original PathToOutputFiles + native scoped root, Unicode/separators, real CFile IO, creation/failure, containment, retained snapshot, unwind/rebind PASS HOST_FILESYSTEM_ONLY NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN");
}
