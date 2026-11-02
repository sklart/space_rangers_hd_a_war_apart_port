// SPDX-License-Identifier: GPL-2.0-or-later
// Differential against real GR_Main config routines; no game-data writes.
#include "rangers_data_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"
#include "units/aPacket.hpp"
#include "ec_file_adapter.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
namespace m=srhd_awa::matrixgame;
void Save(const char* name,const void* data,std::size_t bytes){std::ofstream out(name,std::ios::binary);out.write(static_cast<const char*>(data),bytes);assert(out.good());}
void BlockBytes(EC_BlockPar::TBlockParEC* block,const char* name){
 auto* buffer=pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
 try{block->WriteTextBuffer(buffer,false,true);Save(name,buffer->Data,buffer->DataSize);}catch(...){pas::free(buffer);throw;}
 pas::free(buffer);
}
void Snapshot(){
 BlockBytes(GR_Main::MainDataConfig,"main.bin");BlockBytes(GR_Main::LanguageDataConfig,"language.bin");
 auto* block=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
 try{GR_Main::CacheDataRoot->WriteToBlock(block);BlockBytes(block,"cache.bin");}catch(...){pas::free(block);throw;}pas::free(block);
 std::ofstream state("state.txt",std::ios::binary);
 for(const auto& value:{GR_Main::RequestedLanguage,GR_Main::SelectedLanguage,GR_Main::SelectedMods,GR_Main::SelectedModsDisplaySuffix})state<<static_cast<pas::AnsiString>(value).c_str()<<'\n';
 state<<int(GR_Main::SkipModsOnReload)<<'\n';
 for(auto* pack=EC_HsFile::PackageCollection->FirstPack;pack;pack=pack->NextPack)state<<int(pack->UseLooseFiles)<<':'<<pack->PackagePath.c_str()<<'\n';
 assert(state.good());
}
void Clean(){
 assert(!EC_HsFile::PackageCollection && !EC_HsFile::PackageFileLock && !GR_Main::InstallConfig && !GR_Main::LanguageInstallConfig);
 assert(!GR_Main::ModInstallConfigs && !GR_Main::ModLanguageInstallConfigs && !GR_Main::MainDataConfig && !GR_Main::LanguageDataConfig && !GR_Main::CacheDataRoot && !GR_Main::WideCaseTable.length());
}
void FreeList(pas::List*& list){if(!list)return;for(int i=0;i<pas::list_count(list);++i)pas::free(pas::list_at<pas::Object>(list,i));pas::free(list);list=nullptr;}
int main(int argc,char** argv){
 assert(argc==7);std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);Clean();
 const bool reference=std::strcmp(argv[3],"reference")==0,failure=std::strcmp(argv[3],"failure")==0;
 std::u16string lang;for(const char* c=argv[4];*c;++c)lang.push_back(*c);
 const bool skip=std::strcmp(argv[5],"1")==0;
 GlobalsV::DumpLoadedConfig=false;
 if(reference){
  assert(aPacket::InitializePackageCollection());
  GR_Main::InstallConfig=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::InstallConfig->LoadFromTextFileWithEncodingProbe(pas::literal_pointer(u"INSTALL.TXT"),false);
  GR_Main::RequestedLanguage=pas::WideString(std::u16string_view(lang));GR_Main::SelectedLanguage=pas::WideString();GR_Main::SkipModsOnReload=skip;
  GR_Main::LoadLanguageAndPackages();GR_Main::LoadDatConfigAndModOverrides();Snapshot();
  GR_Main::FreeDatConfigRoots();FreeList(GR_Main::ModInstallConfigs);FreeList(GR_Main::ModLanguageInstallConfigs);
  pas::free(GR_Main::LanguageInstallConfig);GR_Main::LanguageInstallConfig=nullptr;pas::free(GR_Main::InstallConfig);GR_Main::InstallConfig=nullptr;aPacket::FinalizePackageCollection();
 }else{
  GR_Main::RequestedLanguage=u"prior-request"_w;GR_Main::SelectedLanguage=u"prior-selected"_w;GR_Main::SelectedMods=u"prior-mods"_w;GR_Main::SelectedModsDisplaySuffix=u"prior-suffix"_w;GR_Main::SkipModsOnReload=true;
  m::RangersDataOptions options;options.requested_language=lang;options.skip_mods=skip;
  bool rejected=false;
  try{
   m::RangersDataSession data(options);assert(!failure);Snapshot();
   auto* config=&data.MainConfig();assert(config==GR_Main::MainDataConfig && &data.CacheData()==GR_Main::CacheDataRoot && &data.LanguageConfig()==GR_Main::LanguageDataConfig);
   try{m::RangersDataSession duplicate(options);}catch(const std::logic_error&){rejected=true;}
   assert(rejected && GR_Main::MainDataConfig==config && GR_Main::WideCaseTable.length()>0);
  }catch(const std::exception&){assert(failure);rejected=true;}
  if(failure)assert(rejected);
  assert(GR_Main::RequestedLanguage==u"prior-request" && GR_Main::SelectedLanguage==u"prior-selected" && GR_Main::SelectedMods==u"prior-mods" && GR_Main::SelectedModsDisplaySuffix==u"prior-suffix" && GR_Main::SkipModsOnReload);
  if(failure){Clean();m::RangersDataOptions recovery;recovery.requested_language=u"english";{m::RangersDataSession recovered(recovery);assert(GR_Main::SelectedLanguage==u"english");}Clean();}
 }
 Clean();std::printf("RANGERS_DATA mode=%s case=%s cleanup=PASS state_restore=%s\n",argv[3],argv[6],reference?"REFERENCE":"PASS");return 0;
}
