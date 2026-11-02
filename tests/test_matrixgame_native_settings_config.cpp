// SPDX-License-Identifier: GPL-2.0-or-later
// Actual full config/sampler TUs; scalar caps fixture, no graphics provider.
#include "MatrixConfig.hpp"
#include "MatrixGame.h"
#include "MatrixSampleStateManager.hpp"
#include "native_game_settings.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
namespace m=srhd_awa::matrixgame;namespace p=m::platform3g;
int main(int argc,char** argv){
 assert(argc==2);std::ofstream out(argv[1],std::ios::binary);assert(out);
 const auto before=p::Current().caps;unsigned cases{};
 for(unsigned cap:{0u,1u,4u,16u})for(unsigned seed=0;seed<32;++seed){
  auto input=SRobotsSettings::generate_default_settings();input.m_IzvratMS=seed&1;input.m_LandTexturesGloss=seed&2;input.m_ObjTexturesGloss=seed&4;
  input.m_ShowStencilShadows=seed&8;input.m_ShowProjShadows=seed&16;input.m_SoftwareCursor=seed&1;
  input.m_SkyBox=seed%3;input.m_RobotShadow=seed%2;input.m_Brightness=.125f+seed*.015625f;input.m_Contrast=.875f-seed*.015625f;
  input.m_AFDegree=seed;auto prepared=m::PrepareNativeGameSettings(&input,{0,0,1280,720});
  CMatrixConfig config;config.ApplySettings(&prepared);
  assert(config.m_IzvratMS==input.m_IzvratMS && config.m_LandTexturesGloss==input.m_LandTexturesGloss && config.m_ObjTexturesGloss==input.m_ObjTexturesGloss);
  assert(config.m_ShowStencilShadows==input.m_ShowStencilShadows && config.m_ShowProjShadows==input.m_ShowProjShadows && config.m_SoftwareCursor==input.m_SoftwareCursor && config.m_SkyBox==input.m_SkyBox);
  assert(config.m_RobotShadow==(input.m_RobotShadow?SHADOW_STENCIL:SHADOW_OFF));
  for(const auto* ramp:{&config.m_GammaR,&config.m_GammaG,&config.m_GammaB})assert(ramp->brightness==input.m_Brightness && ramp->contrast==input.m_Contrast && ramp->gamma==1.f);
  p::Current().caps.max_anisotropy=cap;CMatrixSampleStateManager sampler;sampler.ApplySettings(&prepared);
  assert(sampler.m_AFDegree==std::min(cap,seed) && input.m_AFDegree==int(seed) && prepared.m_AFDegree==int(seed));
  const unsigned values[]{unsigned(config.m_IzvratMS),unsigned(config.m_LandTexturesGloss),unsigned(config.m_ObjTexturesGloss),unsigned(config.m_ShowStencilShadows),unsigned(config.m_ShowProjShadows),unsigned(config.m_SoftwareCursor),unsigned(config.m_SkyBox),unsigned(config.m_RobotShadow),std::bit_cast<unsigned>(config.m_GammaR.brightness),std::bit_cast<unsigned>(config.m_GammaR.contrast),std::bit_cast<unsigned>(config.m_GammaR.gamma),unsigned(sampler.m_AFDegree)};
  out.write(reinterpret_cast<const char*>(values),sizeof(values));++cases;
 }
 p::Current().caps=before;assert(!p::Current().ready && out.good());
 std::printf("original full-TU ApplySettings=%u sampler_ApplySettings=%u gamma_RGB_exact=PASS original_AF_clamp=PASS caller_unchanged=PASS CPU_SCALAR_CAPS_FIXTURE NO_INIT NO_GPU\n",cases,cases);
}
