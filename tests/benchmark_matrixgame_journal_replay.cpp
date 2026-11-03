#include "native_diagnostic_journal.hpp"
#include "native_profile.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <cassert>
namespace d=srhd_awa::matrixgame::diagnostic;namespace p=srhd_awa::matrixgame::platform3g;
int main(int argc,char** argv){
 assert(argc==4);const bool sampled=std::string(argv[3])=="sampled";std::ifstream input(argv[1]);assert(input);
 const auto start=std::chrono::steady_clock::now();std::uint64_t seen{},written{},pressure{};bool keep_pressure=true;
 {d::Journal journal(std::filesystem::absolute(argv[2]),"HOST_REPLAY","0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
 for(std::string line;std::getline(input,line);){if(!line.empty() && line.back()=='\r')line.pop_back();auto tab=line.find('\t');assert(tab!=line.npos);auto event=line.substr(0,tab),op=line.substr(tab+1);++seen;
  if(event=="pressure"){++pressure;keep_pressure=p::SamplePressure(pressure);}
  if(sampled && ((op=="GPU.batch_submit_wait" && (event=="begin" || event=="ok")) || ((!keep_pressure) && (event=="pressure" || op.starts_with("texture.")))))continue;
  assert(journal.Record(event,op,"host replay: real fwrite/fflush/fsync"));++written;
 }journal.RequireHealthy();assert(journal.Close());}
 const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"HOST_JOURNAL_REPLAY mode="<<(sampled?"sampled":"baseline")<<" input="<<seen<<" written="<<written<<" pressure="<<pressure<<" elapsed_us="<<us<<" REAL_HOST_FILE_IO NOT_SWITCH_MICROSD\n";
}
