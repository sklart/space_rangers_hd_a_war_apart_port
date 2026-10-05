#include "ec_file_adapter.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace {
void u32(FILE* f, uint32_t n) { for (int i=0;i<4;++i) std::fputc((n>>(8*i))&255, f); }
void entry(FILE* f, const char* name, int kind, uint32_t target, uint32_t size) { u32(f,0);u32(f,size);char names[126]{};std::strncpy(names,name,62);std::strncpy(names+63,name,62);std::fwrite(names,1,sizeof(names),f);u32(f,kind);u32(f,kind);u32(f,0);u32(f,0);u32(f,target);u32(f,0); }
bool write_fixture(const char* path) { FILE* f=std::fopen(path,"wb"); if(!f)return false; const char raw[]="plain-data"; const char source[]="compressed-data"; uLong packed_size=compressBound(sizeof(source)-1); std::vector<unsigned char> packed(packed_size); if(compress2(packed.data(),&packed_size,reinterpret_cast<const Bytef*>(source),sizeof(source)-1,Z_BEST_COMPRESSION)!=Z_OK){std::fclose(f);return false;} u32(f,4);u32(f,170);u32(f,2);u32(f,158);entry(f,"TEST.BIN",0,332,sizeof(raw)-1);entry(f,"COMP.BIN",2,346,sizeof(source)-1);u32(f,0);std::fwrite(raw,1,sizeof(raw)-1,f);u32(f,0);u32(f,packed_size+8);std::fwrite("ZL02",1,4,f);u32(f,sizeof(source)-1);std::fwrite(packed.data(),1,packed_size,f);std::fclose(f);return true; }
bool read(const char16_t* path, const char* expected) { EC_File::TFileEC f{}; EC_File::TFileEC_Create(&f);f.SetFileName(pas::WideString(path));if(!f.TryAcquireReadHandle(false)){std::puts("open failed");return false;}if(f.GetSize()!=std::strlen(expected)){std::puts("size failed");return false;}std::vector<char> data(std::strlen(expected));f.ReadBuffer(data.data(),static_cast<uint32_t>(data.size()));bool ok=std::memcmp(data.data(),expected,data.size())==0&&f.SetPointer(2,0)==2&&f.SetPointer(1,1)==3&&f.SetPointer(2,2)==data.size()-2;if(!ok)std::puts("read or seek failed");f.ReleaseHandle();EC_File::TFileEC_Destroy(&f);return ok; }
}
int main(){const char* path="build/ec_file_synthetic.pkg";std::string error;if(!write_fixture(path)||!srhd_awa::platform::ec_file::OpenPackages({path,path},&error))return 1;bool ok=read(u"test.bin","plain-data")&&read(u"COMP.BIN","compressed-data");std::vector<std::int32_t> handles;for(int i=0;i<17;++i){const auto handle=EC_HsFile::PackageCollection->OpenEntryByPathAcrossPackages(pas::AnsiString("TEST.BIN"),0,false);if(handle<0){ok=false;break;}handles.push_back(handle);}const bool composite_handle=handles.size()==17&&handles.back()==16;const bool first_package_limit=EC_HsFile::PackageCollection->OpenEntryByPathAcrossPackages(pas::AnsiString("TEST.BIN"),0,true)==-1;for(const auto handle:handles)EC_HsFile::PackageCollection->CloseEntryHandle(handle);const bool missing_rejected=EC_HsFile::PackageCollection->OpenEntryByPathAcrossPackages(pas::AnsiString("missing.bin"),0,false)==-1;ok=ok&&composite_handle&&first_package_limit&&missing_rejected;srhd_awa::platform::ec_file::ClosePackage();std::remove(path);if(ok)std::puts("synthetic EC_File regression passed");return ok?0:1;}
