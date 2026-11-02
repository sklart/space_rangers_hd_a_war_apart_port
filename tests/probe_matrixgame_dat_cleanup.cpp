// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_HsFile.hpp"
#include "units/aPacket.hpp"
#include "ec_file_adapter.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
 assert(argc==3);std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);assert(aPacket::InitializePackageCollection());
 // This private one-byte fixture is deliberately too short for the first read.
 std::ofstream tiny("config/e573-short.dat",std::ios::binary);tiny.put('\0');tiny.close();
 auto* block=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
 auto* data=pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
 Reject([&]{block->LoadFromEncryptedDatFile(u"e573-not-present.dat"_w);});
 Reject([&]{data->LoadFromEncryptedDatFile(u"e573-not-present.dat"_w);});
 Reject([&]{block->LoadFromEncryptedDatFile(u"e573-short.dat"_w);});
 Reject([&]{data->LoadFromEncryptedDatFile(u"e573-short.dat"_w);});
 assert(!block->GetBlockCount() && !block->GetParamCount() && data->IsEmpty());
 for(int index=0;index<16;++index)assert(EC_HsFile::PackageCollection->FirstPack->OpenSlots[index].IsAvailable);
 pas::free(block);pas::free(data);aPacket::FinalizePackageCollection();assert(!EC_HsFile::PackageCollection && !EC_HsFile::PackageFileLock);
 std::puts("DAT_CLEANUP missing_open=2 short_read=2 exception=PASS handles_closed=PASS NO_FORMAT_CHANGES");return 0;
}
