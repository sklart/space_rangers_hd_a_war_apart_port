#include "material_uniform_packet.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <span>
using namespace srhd_awa::matrixgame::materials;
int main(){
 Uniforms mat{};VertexUniforms vertex{};PixelFogUniforms fog{};
 std::memset(&mat,0x37,sizeof(mat));std::memset(&vertex,0x93,sizeof(vertex));std::memset(&fog,0x51,sizeof(fog));
 for(unsigned shade:{1u,2u})for(bool has_vertex:{false,true})for(unsigned poison:{0u,0x35u,0xffu}){
  std::array<std::byte,uniform_packet::Bytes+32> actual,expected;
  actual.fill(std::byte(poison));expected=actual;
  std::memset(expected.data(),0,uniform_packet::Bytes);
  std::memcpy(expected.data(),&mat,sizeof(mat));
  std::memcpy(expected.data()+uniform_packet::FogOffset,&fog,sizeof(fog));
  const std::array<std::uint32_t,4> controls{shade,0,0,0};
  std::memcpy(expected.data()+uniform_packet::RasterOffset,controls.data(),sizeof(controls));
  if(has_vertex)std::memcpy(expected.data()+uniform_packet::VertexOffset,&vertex,sizeof(vertex));
  uniform_packet::Write(actual,mat,has_vertex?&vertex:nullptr,fog,shade);
  assert(actual==expected);
  std::array<std::byte,uniform_packet::Bytes+32> compact;compact.fill(std::byte(poison));
  uniform_packet::Write(std::span(compact).first(uniform_packet::MinimumBytes),mat,has_vertex?&vertex:nullptr,fog,shade);
  assert(!std::memcmp(compact.data(),expected.data(),uniform_packet::MinimumBytes));
  for(auto x:std::span(compact).subspan(uniform_packet::MinimumBytes))assert(x==std::byte(poison));
 }
 std::array<std::byte,uniform_packet::Bytes> data;data.fill(std::byte{0x71});auto before=data;
 try{uniform_packet::Write(data,mat,&vertex,fog,3);assert(false);}catch(const std::invalid_argument&){}assert(data==before);
 try{uniform_packet::Write(std::span(data).first(1),mat,&vertex,fog,1);assert(false);}catch(const std::length_error&){}assert(data==before);
 try{uniform_packet::Write(std::span(data).first(uniform_packet::Bytes-1),mat,&vertex,fog,1);assert(false);}catch(const std::length_error&){}assert(data==before);
 std::puts("uniform packet exact bytes, bounds, null vertex and validation PASS; CPU ONLY");
}
