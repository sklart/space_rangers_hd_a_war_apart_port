// SPDX-License-Identifier: GPL-2.0-or-later
#include "../port/matrixgame/deko/raster_state.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace srhd_awa::matrixgame;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"raster CHECK %d: %s\n",__LINE__,#e);return 3;}}while(0)
int main(int argc,char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
#endif
  if(argc!=2)return 2;
  MeshRasterState state;
  for(const auto& pair:{std::pair{7u,2u},{15u,2u},{19u,12u},{20u,0u},{23u,9u},{24u,256u},{25u,0u},{168u,16u},{171u,6u},{206u,1u}}) {
    const auto before=state.Words();bool rejected=false;
    try{state.Apply(static_cast<RasterRegister>(pair.first),pair.second);}catch(const std::invalid_argument&){rejected=true;}
    CHECK(rejected&&state.Words()==before);
  }
  auto invalid=state;invalid.alpha_reference=256;bool rejected=false;
  try{invalid.Validate();}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
  std::ifstream manifest(argv[1]);CHECK(bool(manifest));std::uint32_t reg,value;
  const auto dump=[&]{const auto words=state.Words();return std::fwrite(words.data(),1,sizeof(words),stdout)==sizeof(words);};
  CHECK(dump());
  while(manifest>>reg>>value){state.Apply(static_cast<RasterRegister>(reg),value);CHECK(dump());}
  CHECK(manifest.eof());CHECK(!std::fflush(stdout));return 0;
}
