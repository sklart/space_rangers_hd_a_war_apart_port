// Exact CPU layout/bytes used by the real one-block SDK material packet.
#include "material_uniform_packet.hpp"
#include <array>
#include <cassert>
#include <cstdio>
namespace m=srhd_awa::matrixgame::materials;
namespace u=m::uniform_packet;
template<class F>void Reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}assert(bad);}
int main(){
    static_assert(u::Bytes==4096 && u::VertexOffset==1024 && sizeof(m::VertexUniforms)==1328);
    m::Uniforms material{};m::VertexUniforms vertex{};m::PixelFogUniforms fog{};
    std::memset(&material,0x11,sizeof(material));std::memset(&vertex,0x22,sizeof(vertex));std::memset(&fog,0x33,sizeof(fog));
    std::array<std::byte,u::Bytes+64> memory;memory.fill(std::byte{0x6a});
    auto packet=std::span(memory).subspan(32,u::Bytes);
    u::Write(packet,material,&vertex,fog,2);
    assert(!std::memcmp(packet.data(),&material,sizeof(material)));
    assert(!std::memcmp(packet.data()+u::FogOffset,&fog,sizeof(fog)));
    assert(!std::memcmp(packet.data()+u::VertexOffset,&vertex,sizeof(vertex)));
    const std::array<unsigned,4> raster{2,0,0,0};assert(!std::memcmp(packet.data()+u::RasterOffset,raster.data(),16));
    for(unsigned i=0;i<32;++i)assert(memory[i]==std::byte{0x6a} && memory[32+u::Bytes+i]==std::byte{0x6a});
    for(unsigned i=0;i<u::Bytes;++i)if(i>=sizeof(material) && !(i>=u::FogOffset && i<u::FogOffset+sizeof(fog)) &&
        !(i>=u::RasterOffset && i<u::RasterOffset+16) && !(i>=u::VertexOffset && i<u::VertexOffset+sizeof(vertex)))assert(packet[i]==std::byte{});
    const auto first=memory;std::array<std::byte,u::Bytes> second;u::Write(second,material,nullptr,fog,1);
    assert(memory==first);for(unsigned i=u::VertexOffset;i<u::VertexOffset+sizeof(vertex);++i)assert(second[i]==std::byte{});
    auto unchanged=memory;Reject([&]{u::Write(packet.first(u::Bytes-1),material,&vertex,fog,2);});assert(memory==unchanged);
    Reject([&]{u::Write(packet,material,&vertex,fog,0);});assert(memory==unchanged);
    std::puts("MATERIAL_PACKET PASS one_4096_block fragment/fog/raster/vertex bytes_exact independent_packets padding_zero aligned_ranges short_invalid_rejected CPU_ONLY NO_GPU");
}
