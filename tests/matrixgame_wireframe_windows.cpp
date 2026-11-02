// SPDX-License-Identifier: GPL-2.0-or-later
#define MATRIXGAME_FOG_DRIVER_ONLY
#include "matrixgame_fog_windows.cpp"
struct RawVertex {float x,y,z,w;unsigned color;};
int main(int argc,char** argv){
 assert(argc==2);std::setvbuf(stdout,nullptr,_IONBF,0);Hal hal;MaterialGlReference gl(argv[1]);
 mat::FixedVertexState vertex;vertex.Layout(0x44);vertex.Viewport({0,0,16,16,0,1});
 mat::Combiner combiner;combiner.Set(0,1,2);combiner.Set(0,2,0);combiner.Set(0,4,2);combiner.Set(0,5,0);
 srhd_awa::matrixgame::MeshRasterState raster;raster.depth_test=raster.depth_write=false;
 unsigned cases{},mismatches{},max_delta{},differing_pixels{},wire_pixels{},solid_pixels{};
 for(unsigned fill:{3u,2u})for(unsigned primitive:{4u,5u,6u})for(unsigned cull:{1u,2u,3u})for(bool reverse:{false,true})for(unsigned shape=0;shape<4;++shape)for(float shift:{0.f,.125f,.5f,.875f}){
  RawVertex raw[3]{{2+shift,2+shift,.25f,1,0xffd15329},{13+shift,4+shift,.25f,1,0xffd15329},{5+shift,13+shift,.25f,1,0xffd15329}};
  if(shape==1){raw[0].x=-5;raw[1].x=22;raw[2].y=21;}
  if(shape==2){raw[2]=raw[1];}
  if(shape==3){raw[0].x=-8;raw[1].x=-5;raw[2].x=-2;}
  if(reverse)std::swap(raw[0],raw[1]);MaterialMeshOptions options;options.fill=fill;options.primitive=primitive;options.cull=cull;
  const auto bytes=std::as_bytes(std::span{raw});const auto sdk=hal.Draw(0x44,bytes,sizeof(RawVertex),mat::Identity(),{},raster,options);
  DWORD stored{};HR(hal.device->GetRenderState(D3DRS_FILLMODE,&stored));assert(stored==fill);
  const auto native=gl.DrawMesh(vertex.Prepare(),combiner.Prepare(),{},vertex.Decode(bytes,sizeof(RawVertex)),raster,16,16,options);
  unsigned delta{},different{},covered{};for(unsigned i=0;i<256;++i){covered+=sdk[i]!=0x70492795u;different+=sdk[i]!=native[i];for(unsigned c=0;c<4;++c)delta=std::max(delta,unsigned(std::abs(int((sdk[i]>>(c*8))&255)-int((native[i]>>(c*8))&255))));}
  if(fill==2)wire_pixels+=covered;else solid_pixels+=covered;
  if(delta){if(mismatches<8)std::printf("WIRE_DIFFER fill=%u primitive=%u cull=%u reverse=%u shape=%u shift=%.3f delta=%u pixels=%u\n",fill,primitive,cull,reverse,shape,shift,delta,different);++mismatches;}
  differing_pixels+=different;max_delta=std::max(max_delta,delta);++cases;
 }
 assert(wire_pixels>0 && solid_pixels>wire_pixels);
 std::printf("WIREFRAME_HAL_PRODUCTION_GLSL cases=%u mismatches=%u different_pixels=%u max_delta=%u wire_pixels=%u solid_pixels=%u NO_DEKO NO_SWITCH\n",cases,mismatches,differing_pixels,max_delta,wire_pixels,solid_pixels);
 unsigned gradient_cases{},gradient_bad{},gradient_delta{},coverage_bad{},sequence_cases{},sequence_bad{};
 for(unsigned shade:{1u,2u})for(unsigned primitive:{4u,5u,6u})for(bool variable_w:{false,true})for(bool reverse:{false,true})for(float shift:{0.f,.125f,.5f,.875f}){
  RawVertex raw[3]{{2+shift,2+shift,.25f,1,0xffe03123},{13+shift,4+shift,.25f,variable_w?.5f:1,0xff31c541},{5+shift,13+shift,.25f,variable_w?.25f:1,0xff273fd1}};
  if(reverse)std::swap(raw[0],raw[1]);MaterialMeshOptions options;options.fill=2;options.primitive=primitive;options.shade=shade;
  const auto bytes=std::as_bytes(std::span{raw});const auto sdk=hal.Draw(0x44,bytes,sizeof(RawVertex),mat::Identity(),{},raster,options);
  const auto native=gl.DrawMesh(vertex.Prepare(),combiner.Prepare(),{},vertex.Decode(bytes,sizeof(RawVertex)),raster,16,16,options);
  unsigned delta{},coverage{};for(unsigned i=0;i<256;++i){coverage+=(sdk[i]==0x70492795u)!=(native[i]==0x70492795u);for(unsigned c=0;c<4;++c)delta=std::max(delta,unsigned(std::abs(int((sdk[i]>>(c*8))&255)-int((native[i]>>(c*8))&255))));}
  coverage_bad+=coverage;gradient_bad+=delta>2;gradient_delta=std::max(gradient_delta,delta);++gradient_cases;
 }
 // Source-derived selection stencil sequence with controlled geometry. The
 // full CMatrixMap::Draw/CMultiSelection bodies are not executed by this probe.
 for(bool reverse:{false,true})for(float shift:{0.f,.125f,.5f,.875f}){
  RawVertex raw[3]{{2+shift,2+shift,.25f,1,0xffd15329},{13+shift,4+shift,.25f,1,0xffd15329},{5+shift,13+shift,.25f,1,0xffd15329}};
  if(reverse)std::swap(raw[0],raw[1]);const auto bytes=std::as_bytes(std::span{raw});const auto decoded=vertex.Decode(bytes,sizeof(RawVertex));
  MaterialMeshOptions options;options.stencil.enabled=true;options.stencil.reference=1;options.stencil.clockwise={1,1,1,8};options.clear_stencil=1;
  // DrawPass2: EQUAL + ZERO + WIREFRAME. Both color and remaining stencil must match.
  options.stencil.clockwise.compare=3;options.stencil.clockwise.pass=2;options.fill=2;
  const auto a=hal.Draw(0x44,bytes,sizeof(RawVertex),mat::Identity(),{},raster,options);
  const auto b=gl.DrawMesh(vertex.Prepare(),combiner.Prepare(),{},decoded,raster,16,16,options);
  bool bad=a!=b;
  // DrawPassEnd restores SOLID and disables stencil; preserve the first image.
  options.fill=3;options.stencil.enabled=false;options.preserve=true;
  const auto c=hal.Draw(0x44,bytes,sizeof(RawVertex),mat::Identity(),{},raster,options);
  const auto d=gl.DrawMesh(vertex.Prepare(),combiner.Prepare(),{},decoded,raster,16,16,options);
  bad|=c!=d;
  // HAL's stencil-to-color resolve is destructive to color; perform it only
  // after the SOLID continuation. That pass has stencil disabled, preserving
  // the ZERO results produced by the preceding wireframe pass.
  bad|=hal.ReadStencil()!=gl.ReadStencil();sequence_bad+=bad;++sequence_cases;
 }
 std::printf("WIREFRAME_GRADIENT cases=%u mismatches=%u coverage_mismatches=%u max_delta=%u; SELECTION_STENCIL_SEQUENCE cases=%u mismatches=%u\n",gradient_cases,gradient_bad,coverage_bad,gradient_delta,sequence_cases,sequence_bad);
 return mismatches||gradient_bad||coverage_bad||sequence_bad?10:0;
}
