#ifdef NDEBUG
#error WAV compatibility verification requires assertions
#endif
#include "e2e4_wav.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
using namespace srhd_awa::platform::e2e4_wav;
int main(int argc,char** argv){
 assert(argc==2);std::ifstream file(argv[1],std::ios::binary);assert(file);
 std::vector<unsigned char> data{std::istreambuf_iterator<char>(file),{}};
 assert(data.size()==24748 && Read32(data.data()+4)==data.size());
 const auto strict=Parse(data.data(),data.size());assert(!strict && std::strcmp(strict.error,"truncated RIFF")==0);
 const auto legacy=Parse(data.data(),data.size(),0,RiffSizePolicy::LegacyTotalBytes);
 assert(legacy && legacy.legacy_riff_size && legacy.offset==44 && legacy.bytes==24704 && legacy.next_position==data.size());
 assert(legacy.channels==1 && legacy.bits==8 && legacy.rate==11025 && legacy.block_align==1);
 for(unsigned remove:{1u,8u,100u})assert(!Parse(data.data(),data.size()-remove,0,RiffSizePolicy::LegacyTotalBytes));
 auto malformed=data;malformed[40]++;assert(!Parse(malformed.data(),malformed.size(),0,RiffSizePolicy::LegacyTotalBytes));
 malformed=data;malformed[20]=3;assert(!Parse(malformed.data(),malformed.size(),0,RiffSizePolicy::LegacyTotalBytes));
 malformed=data;malformed.push_back(0);assert(!Parse(malformed.data(),malformed.size(),0,RiffSizePolicy::LegacyTotalBytes));
 auto standard=data;const auto size=standard.size()-8;for(unsigned i=0;i<4;++i)standard[4+i]=static_cast<unsigned char>(size>>(8*i));
 const auto normal=Parse(standard.data(),standard.size());assert(normal && !normal.legacy_riff_size);
 const auto compatible=Parse(standard.data(),standard.size(),0,RiffSizePolicy::LegacyTotalBytes);assert(compatible && !compatible.legacy_riff_size);
 assert(std::memcmp(standard.data()+normal.offset,data.data()+legacy.offset,legacy.bytes)==0);
 std::vector<unsigned char> prefixed{1,2,3};prefixed.insert(prefixed.end(),data.begin(),data.end());
 const auto embedded=Parse(prefixed.data(),prefixed.size(),3,RiffSizePolicy::LegacyTotalBytes);assert(embedded && embedded.offset==47 && embedded.next_position==prefixed.size());
 std::puts("LEGACY_WAV original_Sound_Flame=PASS strict_rejection_unchanged=PASS explicit_total_size_compat=PASS exact_PCM=PASS malformed_bounds=PASS");
}
