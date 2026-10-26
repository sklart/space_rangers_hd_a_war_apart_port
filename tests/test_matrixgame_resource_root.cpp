// SPDX-License-Identifier: GPL-2.0-or-later
#include "CFile.hpp"
#include "CBlockPar.hpp"
#include "CStorage.hpp"
#include "CException.hpp"
#include <cassert>
#include <cstdio>
#include <codecvt>
#include <locale>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <vector>
#if MATRIXGAME_RESOURCE_ROOT_CANDIDATE
#include "resource_paths.hpp"
#endif
namespace fs=std::filesystem;
std::wstring Wide(const fs::path& path){
#ifdef _WIN32
 return path.wstring();
#else
 return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(path.native());
#endif
}
std::string Read(const wchar_t* name){Base::CFile file(name);file.OpenRead();assert(file.SizeFull()<64*1024*1024);std::string data(file.SizeFull(),'\0');file.Read(data.data(),dword(data.size()));return data;}
template<class F>void Fails(F f){bool failed=false;try{f();}catch(...){failed=true;}assert(failed);}
std::vector<std::wstring> enumerated;
void Found(const std::wstring& name,dword user){assert(user==12345);enumerated.push_back(name);}
int main(int argc,char** argv){assert(argc==4);const auto real=fs::absolute(argv[1]),foreign=fs::absolute(argv[2]),unit=fs::absolute(argv[3]);
 assert(fs::is_regular_file(real/"DATA/robots.pkg") && fs::is_directory(foreign));fs::current_path(foreign);const auto cwd=fs::current_path();Base::CFile::StaticInit();
 const char* stage="CGame::Init->CFile::OpenPackFiles";
 try{
#if MATRIXGAME_RESOURCE_ROOT_CANDIDATE
  {
   srhd_awa::matrixgame::resource_paths::NativeScope input(Wide(real).c_str());assert(fs::current_path()==cwd);
#endif
   Base::CFile::AddPackFile(L"DATA\\robots.pkg");Base::CFile::OpenPackFiles();
   {stage="CBlockPar::LoadFromTextFile";Base::CBlockPar config;config.LoadFromTextFile(L"cfg\\robots\\data.txt");assert(config.BlockCount()>0);
    stage="CStorage::Load";const auto map_name=config.BlockGet(L"Config")->ParGet(L"Map");Base::CStorage map;assert(map.Load(map_name.c_str()));
    std::printf("REAL_RESOURCE_LOAD original_CFG=PASS original_CStorage_Load=PASS map_id=%08x NO_PREPAREMAP NO_GPU\n",map.CalcUniqID());}
   Base::CFile::ReleasePackFiles();
#if MATRIXGAME_RESOURCE_ROOT_CANDIDATE
  }
  namespace r=srhd_awa::matrixgame::resource_paths;
  stage="resource scope contract";
  assert(!r::Bound() && fs::current_path()==cwd);
  Fails([&]{r::NativeScope invalid(L"relative");});assert(!r::Bound());
  assert(r::PlanReadPath("sdmc:/game","DATA/robots.pkg")==fs::path("sdmc:/game/DATA/robots.pkg"));
  assert(r::PlanReadPath("sdmc:/game","sdmc:/private/cache")==fs::path("sdmc:/private/cache"));
  {
   r::NativeScope input(Wide(unit).c_str());Fails([&]{r::NativeScope nested(Wide(real).c_str());});assert(r::Bound());
   Base::CFile::AddPackFile(L"DATA\\robots.pkg");Base::CFile::OpenPackFiles();
   assert(Read(L"MUSIC")=="loose");fs::remove(unit/"MUSIC");
   const auto packed=Read(L"MUSIC");assert(packed.size()==1100003 && static_cast<unsigned char>(packed[1])==17);
   std::wstring found;assert(Base::CFile::FileExist(found,L"cFg\\thing?alpha=1",L"cfg",true) && found==L"cFg\\Thing.CfG?alpha=1");
   assert(Read(L"cFg\\Thing.CfG")=="root-file");Base::CFile::FindFiles(L"cFg",L"*.cfg",Found,12345);
   assert(enumerated==std::vector<std::wstring>{L"cFg/Thing.CfG"});
   Base::CFile disk_only(L"MUSIC");assert(!disk_only.OpenReadNE());
   std::atomic<unsigned> reads{};auto reader=[&]{for(unsigned i=0;i<8;++i)if(Read(L"cFg/Thing.CfG")=="root-file")++reads;};
   std::thread a(reader),b(reader);a.join();b.join();assert(reads==16);
   {Base::CFile file(L"created.bin");file.Create();char byte='x';file.Write(&byte,1);}assert(fs::exists(cwd/"created.bin") && !fs::exists(unit/"created.bin"));
   assert(Read(Wide(cwd/"created.bin").c_str())=="x");Base::CFile::ReleasePackFiles();
  }
  assert(!r::Bound() && fs::current_path()==cwd);std::wstring ignored;assert(!Base::CFile::FileExist(ignored,L"cFg/Thing.CfG"));
  std::puts("RESOURCE_ROOT PASS foreign_cwd_unchanged package_keys disk_first case_extensions enumeration worker_reads=16 absolute_cache write_destinations scope_cleanup CPU_ONLY SWITCH_HARDWARE_NOT_RUN");return 0;
#else
  std::puts("BASELINE_UNEXPECTED_ROOTLESS_SUCCESS");return 3;
#endif
 }catch(const Base::CException& error){Base::CFile::ReleasePackFiles();std::printf("RESOURCE_ROOT_STOP source=%s CException\n",stage);return 10;}
 catch(const std::exception& error){Base::CFile::ReleasePackFiles();std::printf("RESOURCE_ROOT_STOP source=%s %s\n",stage,error.what());return 10;}
}
