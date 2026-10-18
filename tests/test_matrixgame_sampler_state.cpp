// SPDX-License-Identifier: GPL-2.0-or-later
#include "../port/matrixgame/deko/sampler_state.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace srhd_awa::matrixgame;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"sampler CHECK %d: %s\n",__LINE__,#e);return 3;}}while(0)
int main(int argc,char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
#endif
  if(argc!=2)return 2;
  MeshSamplerState state;
  for(const auto& pair:{std::pair{1u,2u},{2u,0u},{3u,4u},{5u,0u},{6u,3u},{7u,3u},{8u,0x7f800000u},{8u,0x7fc00000u},{8u,0xff800000u},{9u,1u},{4u,0u}}) {
    const auto before=state.Words();bool rejected=false;
    try{state.Apply(static_cast<SamplerRegister>(pair.first),pair.second);}catch(const std::invalid_argument&){rejected=true;}
    CHECK(rejected&&state.Words()==before);
  }
  auto invalid=state;invalid.lod_bias=std::bit_cast<float>(0x7fc00000u);bool rejected=false;
  try{invalid.Validate();}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
  for(unsigned i=0;i<8;++i) {
    MeshSamplerState preset{static_cast<TextureSampling>(i)};preset.Validate();
    CHECK(preset.min==preset.mag && preset.u==preset.v && preset.u==preset.w && preset.lod_bias==0);
    CHECK(unsigned(preset.min)==1+(i&1) && unsigned(preset.u)==((i&2)?3u:1u));
    CHECK(unsigned(preset.mip)==((i&4)?1+(i&1):0));
  }
  bool bad_preset=false;try{MeshSamplerState bad{static_cast<TextureSampling>(8)};}catch(const std::invalid_argument&){bad_preset=true;}CHECK(bad_preset);
  std::ifstream manifest(argv[1]);CHECK(bool(manifest));std::uint32_t reg,value;
  const auto dump=[&]{const auto words=state.Words();return std::fwrite(words.data(),1,sizeof(words),stdout)==sizeof(words);};
  CHECK(dump());
  while(manifest>>reg>>value){state.Apply(static_cast<SamplerRegister>(reg),value);CHECK(dump());}
  CHECK(manifest.eof());CHECK(!std::fflush(stdout));return 0;
}
