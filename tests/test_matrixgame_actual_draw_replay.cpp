// SPDX-License-Identifier: GPL-2.0-or-later
#include "matrixgame_actual_draw_state.hpp"
#include <cstdio>
#include <map>
#include <string>
int main(int argc,char** argv){
 using namespace matrixgame_draw_audit;if(argc<2)return 2;
 std::map<std::string,unsigned> failures;std::uint64_t total{};unsigned unique{};
 for(int a=1;a<argc;++a){std::ifstream file(argv[a],std::ios::binary);std::array<std::uint32_t,4> header{};file.read(reinterpret_cast<char*>(header.data()),sizeof(header));
  if(!file || header[0]!=Magic || header[1]!=1 || header[2]!=sizeof(State) || header[3]>100000)return 3;
  for(unsigned i=0;i<header[3];++i){State state{};std::uint64_t count{};file.read(reinterpret_cast<char*>(&state),sizeof(state));file.read(reinterpret_cast<char*>(&count),sizeof(count));if(!file || !count)return 4;
   total+=count;++unique;try{Validate(state);}catch(const std::exception& e){++failures[e.what()];}}
  if(file.peek()!=std::char_traits<char>::eof())return 5;
 }
 std::printf("actual HAL map state replay draws=%llu unique=%u failures=%zu CPU_ONLY NO_DEKO_GPU NO_SWITCH_HARDWARE\n",static_cast<unsigned long long>(total),unique,failures.size());
 for(const auto& [error,count]:failures)std::printf("REJECT unique=%u reason=%s\n",count,error.c_str());
 return total && failures.empty()?0:10;
}
