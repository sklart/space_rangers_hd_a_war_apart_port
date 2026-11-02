// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_game_settings.hpp"
#include <array>
#include <bit>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>
#include <type_traits>
#include <vector>
namespace m=srhd_awa::matrixgame;
// Every scalar in the actual SRobotsSettings declaration; no padding/pointers.
#define SETTINGS_FIELDS(X) X(m_ShowStencilShadows) X(m_ShowProjShadows) X(m_IzvratMS) X(m_LandTexturesGloss) X(m_ObjTexturesGloss) X(m_SoftwareCursor) X(m_SkyBox) X(m_RobotShadow) X(m_BPP) X(m_ResolutionX) X(m_ResolutionY) X(m_RefreshRate) X(m_Brightness) X(m_Contrast) X(m_FSAASamples) X(m_AFDegree) X(m_MaxDistance) X(m_VSync)
std::vector<unsigned> Observe(const SRobotsSettings& s){
 std::vector<unsigned> result;
 auto word=[&](auto value){using T=decltype(value);if constexpr(std::is_same_v<T,float>)result.push_back(std::bit_cast<unsigned>(value));else result.push_back(unsigned(value));};
#define APPEND(name) word(s.name);
 SETTINGS_FIELDS(APPEND)
#undef APPEND
 assert(!s.FDirect3D && !s.FD3DDevice && result.size()==18);return result;
}
template<class F>void Reject(F&& f){bool failed=false;try{f();}catch(const std::invalid_argument&){failed=true;}assert(failed);}
int main(int argc,char** argv){
 assert(argc==2 && std::endian::native==std::endian::little);std::ofstream output(argv[1],std::ios::binary);assert(output);
 unsigned defaults{},provided{},rejected{};
 for(auto extent:{std::array{1280,720},std::array{800,600},std::array{1920,1080},std::array{32768,1}}){
  const m::platform3g::WindowRect rect{17,-9,17+extent[0],-9+extent[1]};
  auto expected=SRobotsSettings::generate_default_settings();expected.m_ResolutionX=extent[0];expected.m_ResolutionY=extent[1];
  const auto actual=m::PrepareNativeGameSettings(nullptr,rect);assert(Observe(actual)==Observe(expected));++defaults;
  for(unsigned seed=0;seed<16;++seed){
   auto settings=SRobotsSettings::generate_default_settings();
   settings.m_ShowStencilShadows=seed&1;settings.m_ShowProjShadows=seed&2;settings.m_IzvratMS=seed&4;
   settings.m_LandTexturesGloss=seed&8;settings.m_ObjTexturesGloss=!(seed&1);settings.m_SoftwareCursor=seed&2;
   settings.m_SkyBox=seed%3;settings.m_RobotShadow=seed%2;settings.m_BPP=seed&1?16:32;
   settings.m_RefreshRate=seed&1?60:0;settings.m_Brightness=.1f+seed*.03f;settings.m_Contrast=.9f-seed*.02f;
   settings.m_FSAASamples=seed%5;settings.m_AFDegree=seed*3;settings.m_MaxDistance=.25f+seed*.1f;settings.m_VSync=seed&4;
   if(seed&8){settings.m_ResolutionX=extent[0];settings.m_ResolutionY=extent[1];}
   const auto before=Observe(settings);auto copy=m::PrepareNativeGameSettings(&settings,rect);
   assert(Observe(settings)==before);auto wanted=settings;wanted.m_ResolutionX=extent[0];wanted.m_ResolutionY=extent[1];
   const auto words=Observe(copy);assert(words==Observe(wanted));output.write(reinterpret_cast<const char*>(words.data()),words.size()*4);++provided;
  }
 }
 const m::platform3g::WindowRect rect{0,0,1280,720};
 for(auto dimensions:{std::array{0,720},std::array{1280,0},std::array{-1,720},std::array{800,600}}){auto value=SRobotsSettings::generate_default_settings();value.m_ResolutionX=dimensions[0];value.m_ResolutionY=dimensions[1];Reject([&]{m::PrepareNativeGameSettings(&value,rect);});++rejected;}
 for(unsigned which=0;which<2;++which){auto value=SRobotsSettings::generate_default_settings();(which?value.FD3DDevice:value.FDirect3D)=reinterpret_cast<void*>(std::uintptr_t(1));Reject([&]{m::PrepareNativeGameSettings(&value,rect);});++rejected;}
 for(auto member:{&SRobotsSettings::m_Brightness,&SRobotsSettings::m_Contrast,&SRobotsSettings::m_MaxDistance})for(float invalid:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  auto value=SRobotsSettings::generate_default_settings();value.*member=invalid;Reject([&]{m::PrepareNativeGameSettings(&value,rect);});++rejected;}
 for(auto bad:{m::platform3g::WindowRect{0,0,0,720},m::platform3g::WindowRect{0,0,1280,-1},m::platform3g::WindowRect{0,0,32769,720},m::platform3g::WindowRect{std::numeric_limits<int>::min(),0,std::numeric_limits<int>::max(),720}}){Reject([&]{m::PrepareNativeGameSettings(nullptr,bad);});++rejected;}
 assert(output.good());std::printf("native settings defaults=%u provided=%u rejected=%u scalar_fields=18 caller_unchanged=PASS quality_requests_preserved=PASS CPU_SETTINGS_ONLY NO_INIT NO_GPU\n",defaults,provided,rejected);
}
