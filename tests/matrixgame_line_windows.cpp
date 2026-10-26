// SPDX-License-Identifier: GPL-2.0-or-later
#define MATRIXGAME_FOG_DRIVER_ONLY
#include "matrixgame_fog_windows.cpp"
#include <cmath>
#include "matrixgame_actual_draw_state.hpp"
#include <string_view>
struct RawLineVertex {float x,y,z,w;unsigned color;};
int main(int argc,char** argv){
 assert(argc>=2);std::setvbuf(stdout,nullptr,_IONBF,0);Hal hal;MaterialGlReference gl(argv[1]);
 const bool actual_only=argc>2 && std::string_view(argv[2])=="--actual-only";
 mat::FixedVertexState vertex;vertex.Layout(0x44);vertex.Viewport({0,0,16,16,0,1});
 mat::Combiner combiner;combiner.Set(0,1,2);combiner.Set(0,2,0);combiner.Set(0,4,2);combiner.Set(0,5,0);
 srhd_awa::matrixgame::MeshRasterState raster;raster.depth_test=raster.depth_write=false;
 unsigned cases{},mismatches[2]{},strip_cases{},degenerate_cases{},max_delta{};
 auto check=[&](std::span<const RawLineVertex> raw,const MaterialMeshOptions& options,bool gradient=false){
  const auto bytes=std::as_bytes(raw);const auto sdk=hal.Draw(0x44,bytes,sizeof(RawLineVertex),mat::Identity(),{},raster,options);
  DWORD stored{};HR(hal.device->GetRenderState(D3DRS_LASTPIXEL,&stored));assert(stored==unsigned(options.last_pixel));
  const auto native=gl.DrawMesh(vertex.Prepare(),combiner.Prepare(),{},vertex.Decode(bytes,sizeof(RawLineVertex)),raster,16,16,options);
  unsigned delta{};for(unsigned i=0;i<256;++i)for(unsigned c=0;c<4;++c)delta=std::max(delta,unsigned(std::abs(int((sdk[i]>>(c*8))&255)-int((native[i]>>(c*8))&255))));
  if(delta>(gradient?2u:0u)){if(mismatches[options.last_pixel]<3)std::printf("LINE_DIFFER case=%u last=%u topology=%u blend=%u gradient=%u delta=%u\n",cases,options.last_pixel,options.primitive,raster.blend_enable,gradient,delta);++mismatches[options.last_pixel];}
  max_delta=std::max(max_delta,delta);++cases;strip_cases+=options.primitive==3;
 };
 if(!actual_only){
 for(bool last:{false,true})for(unsigned angle=0;angle<16;++angle)for(float fraction:{0.f,.125f,.5f,.875f})for(bool reversed:{false,true}){
  const float a=float(angle)*3.14159265358979323846f/8;
  RawLineVertex raw[2]{{8+fraction,8+fraction,.25f,1,0xffd15329},{8+6*std::cos(a)+fraction,8+6*std::sin(a)+fraction,.25f,1,0xffd15329}};
  if(reversed)std::swap(raw[0],raw[1]);
  MaterialMeshOptions options;options.primitive=2;options.last_pixel=last;
  check(raw,options);
 }
 for(bool last:{false,true})for(unsigned primitive:{2u,3u})for(bool blend:{false,true})for(unsigned shade:{1u,2u})for(bool gradient:{false,true})for(float offset:{0.f,.125f,.5f,.875f}){
  raster.blend_enable=blend;MaterialMeshOptions options;options.primitive=primitive;options.last_pixel=last;options.shade=shade;
  RawLineVertex raw[6]{{-4+offset,3.5f+offset,.25f,1,0x803151c9},{5+offset,-5+offset,.25f,.5f,0x803151c9},{18+offset,9+offset,.25f,.75f,0x803151c9},{3+offset,18+offset,.25f,1.25f,0x803151c9},{-4+offset,3.5f+offset,.25f,1,0x803151c9},{-4+offset,3.5f+offset,.25f,1,0x803151c9}};
  if(gradient)for(unsigned i=0;i<6;++i)raw[i].color=0x80000000u|(i*37u<<16)|((255-i*31u)<<8)|(i*23u);
  const auto count=primitive==2?6u:5u;check({raw,count},options,gradient);
 }
 for(bool last:{false,true})for(float coordinate:{0.f,.25f,.5f,7.f,7.5f,15.f,16.f}){
  raster.blend_enable=false;MaterialMeshOptions options;options.primitive=2;options.last_pixel=last;RawLineVertex raw[2]{{coordinate,coordinate,.25f,1,0xffd15329},{coordinate,coordinate,.25f,1,0xffd15329}};check(raw,options);++degenerate_cases;
 }
 std::printf("RAW_LINE_HAL_GLSL cases=%u strips=%u degenerate=%u last0_mismatches=%u last1_mismatches=%u max_delta=%u NO_SWITCH_HARDWARE\n",cases,strip_cases,degenerate_cases,mismatches[0],mismatches[1],max_delta);
 }else std::puts("RAW_LINE_HAL_GLSL SKIPPED addressed cases already proved in E2E-5.37");
 // Half-pixel viewport placement and logical clipping apply to every primitive.
 // Compare the entire target, including pixels outside the smaller viewport.
 unsigned clip_cases{},clip_mismatches{};
 const D3DVIEWPORT9 clip_vp{3,4,8,7,0,1};vertex.Viewport({3,4,8,7,0,1});raster.blend_enable=false;
 for(unsigned primitive:{4u,5u,6u})for(float offset:{-.25f,0.f,.125f,.5f})for(bool reverse:{false,true}){
  RawLineVertex raw[3]{{-3+offset,-3+offset,.25f,1,0xff30ffff},{25+offset,2+offset,.25f,1,0xff30ffff},{4+offset,24+offset,.25f,1,0xff30ffff}};
  if(reverse)std::swap(raw[0],raw[1]);MaterialMeshOptions options;options.primitive=primitive;
  const auto bytes=std::as_bytes(std::span{raw});const auto sdk=hal.Draw(0x44,bytes,sizeof(RawLineVertex),mat::Identity(),{},raster,options,nullptr,&clip_vp);
  const auto native=gl.DrawMesh(vertex.Prepare(),combiner.Prepare(),{},vertex.Decode(bytes,sizeof(RawLineVertex)),raster,16,16,options);
  if(sdk!=native)++clip_mismatches;++clip_cases;
 }
 std::printf("VIEWPORT_TRIANGLE_CLIP_HAL_GLSL cases=%u mismatches=%u\n",clip_cases,clip_mismatches);
 unsigned actual_cases{},actual_mismatches{},actual_delta{};std::uint64_t actual_pixels{};
 std::unique_ptr<Hal> actual;
 for(int arg=actual_only?3:2;arg<argc;++arg){
  std::ifstream input(argv[arg],std::ios::binary);std::array<std::uint32_t,4> header{};input.read(reinterpret_cast<char*>(header.data()),sizeof(header));
  assert(input && header[0]==0x374c524d && header[1]==1 && header[2]==sizeof(matrixgame_draw_audit::State) && header[3]<=1000);
  for(unsigned i=0;i<header[3];++i){matrixgame_draw_audit::State state{};std::array<std::uint32_t,6> info{};input.read(reinterpret_cast<char*>(&state),sizeof(state));input.read(reinterpret_cast<char*>(info.data()),sizeof(info));
   assert(input && info[2]>0 && info[2]<=4096 && info[3]>0 && info[3]<=4096 && info[5]<=1024*1024);
   std::vector<std::byte> bytes(info[5]);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());assert(input && state.topology==3 && info[0]==4 && info[1]==16 && bytes.size()==80 && state.fvf==4 && info[4]==21);
   matrixgame_draw_audit::Validate(state);auto native_state=matrixgame_draw_audit::Build(state);
   MaterialMeshOptions options;options.primitive=state.topology;options.last_pixel=state.render[5]!=0;options.shade=native_state.shade;options.cull=native_state.cull;options.stencil=native_state.Stencil();
   if(!actual || actual->width!=info[2] || actual->height!=info[3])actual=std::make_unique<Hal>(info[2],info[3]);const auto vp=state.viewport;const D3DVIEWPORT9 sdk_vp{vp.X,vp.Y,vp.Width,vp.Height,vp.MinZ,vp.MaxZ};
   const auto material=native_state.combiner.Prepare();const auto fog=native_state.PixelFog();
   const auto sdk=actual->Draw(state.fvf,bytes,info[1],native_state.vertex.Transform(3),fog,native_state.raster,options,&material,&sdk_vp);
   const auto shader=gl.DrawMesh(native_state.vertex.Prepare(),material,fog,native_state.vertex.Decode(bytes,info[1]),native_state.raster,info[2],info[3],options);
   unsigned delta{};for(unsigned pixel=0;pixel<sdk.size();++pixel)for(unsigned c=0;c<4;++c)delta=std::max(delta,unsigned(std::abs(int((sdk[pixel]>>(8*c))&255)-int((shader[pixel]>>(8*c))&255))));
   if(delta){++actual_mismatches;std::printf("ACTUAL_MINIMAP_LINE_DIFFER case=%u delta=%u capture=%s local_case=%u viewport=%u,%u,%u,%u\n",actual_cases,delta,argv[arg],i,vp.X,vp.Y,vp.Width,vp.Height);
    std::array<float,20> raw{};std::memcpy(raw.data(),bytes.data(),bytes.size());
    for(unsigned v=0;v<5;++v)std::printf(" LINE_VERTEX %u %.9g %.9g %.9g %.9g\n",v,raw[v*4],raw[v*4+1],raw[v*4+2],raw[v*4+3]);
    unsigned different{};for(unsigned pixel=0;pixel<sdk.size();++pixel)if(sdk[pixel]!=shader[pixel]){
     if(different<8)std::printf(" LINE_PIXEL x=%u y=%u HAL=%08x GLSL=%08x\n",pixel%info[2],pixel/info[2],sdk[pixel],shader[pixel]);++different;}
    std::printf(" LINE_DIFFERENT_PIXELS %u\n",different);
   }
   ++actual_cases;actual_pixels+=sdk.size();actual_delta=std::max(actual_delta,delta);
  }
  assert(input.peek()==std::char_traits<char>::eof());
 }
 std::printf("ACTUAL_ORIGINAL_LINE_HAL_GLSL cases=%u pixels=%llu mismatches=%u max_delta=%u NO_SWITCH_HARDWARE\n",actual_cases,static_cast<unsigned long long>(actual_pixels),actual_mismatches,actual_delta);
 return mismatches[0]||mismatches[1]||clip_mismatches||actual_mismatches?10:0;
}
