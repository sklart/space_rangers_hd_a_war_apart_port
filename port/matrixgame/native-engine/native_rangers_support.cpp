// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#elif defined(__SWITCH__)
#include "platform_switch.hpp"
#endif
#include "native_rangers_support.hpp"
#include "platform_3g.hpp"
namespace srhd_awa::matrixgame {
RangersSupportFacts QueryRangersSupport(platform3g::Device& device){
#ifdef _WIN32
 if(auto* reference=dynamic_cast<platform3g::D3D9Device*>(&device)){
  const auto handles=reference->ReadLegacyHandles();
  auto* sdk=reinterpret_cast<IDirect3DDevice9*>(handles.device);
  D3DCAPS9 caps{};if(!sdk || FAILED(sdk->GetDeviceCaps(&caps)))return {};
  return {true,caps.MaxSimultaneousTextures,caps.MaxTextureWidth,caps.MaxStreams};
 }
#elif defined(__SWITCH__)
 if(auto* native=dynamic_cast<platform3g::DekoDevice*>(&device)){
  if(!native->Diagnostics().queue_healthy)return {};
  const auto caps=native->ReadCaps(); // Requires the real material vertex program.
  // Actual port limits: native_texture.cpp rejects dimensions above 8192;
  // BindStagingVertex accepts stream zero only. These are renderer capabilities,
  // not invented hardware VRAM/stream counts or successful allocation promises.
  return {true,caps.max_simultaneous_textures,8192,1};
 }
#endif
 throw platform3g::Unsupported("registered renderer has no real Rangers support query");
}
}
