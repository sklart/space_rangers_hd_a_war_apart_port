// Negative backend proof using a real source PNG referenced by the selected map.
#include "native_texture.hpp"
#include <fstream>
#include <cassert>
#include <cstdio>
namespace nt=srhd_awa::matrixgame::native_texture;
int main(int argc,char** argv){assert(argc==2);std::ifstream file(argv[1],std::ios::binary|std::ios::ate);assert(file);auto size=file.tellg();assert(size>0);std::vector<std::byte> bytes(std::size_t(size),std::byte{});file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());assert(file);
    auto session=std::make_shared<nt::Session>();unsigned uploads=0;session->upload=[&](const nt::Texture&)->std::shared_ptr<nt::Image>{++uploads;throw std::runtime_error("unexpected GPU upload in negative boundary proof");};
    IDirect3DTexture9* out{};bool rejected=false;
    try{nt::Decode(session,bytes.data(),bytes.size(),6,0x31545844,1,&out);}
    catch(const srhd_awa::matrixgame::platform3g::Unsupported& error){rejected=std::string(error.what())=="native texture formatted decode conversion";std::printf("exact blocker: %s\n",error.what());}
    assert(rejected && !out && uploads==0 && nt::HandleCount()==0);
    std::puts("original map source PNG -> RGBA/DXT1 request rejected before publication/upload; no Init/PrepareMap execution claimed");
}
