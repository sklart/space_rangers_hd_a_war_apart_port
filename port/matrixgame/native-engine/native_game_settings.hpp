// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "MatrixGameDll.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame {
// Settings are caller-owned input, never writable storage for original Init.
// Native presentation is already owned by the supplied window/device. A caller
// requesting another extent must resize that presentation before entry.
inline SRobotsSettings PrepareNativeGameSettings(const SRobotsSettings* input,platform3g::WindowRect drawable){
    const auto width=std::int64_t(drawable.right)-drawable.left;
    const auto height=std::int64_t(drawable.bottom)-drawable.top;
    if(width<=0 || height<=0 || width>32768 || height>32768)
        throw std::invalid_argument("native drawable extent invalid");
    auto settings=input?*input:SRobotsSettings::generate_default_settings();
    if(settings.FDirect3D || settings.FD3DDevice)
        throw std::invalid_argument("native settings reject legacy D3D identities");
    if(!settings.m_ResolutionX && !settings.m_ResolutionY){
        settings.m_ResolutionX=static_cast<int>(width);settings.m_ResolutionY=static_cast<int>(height);
    }else if(settings.m_ResolutionX!=width || settings.m_ResolutionY!=height)
        throw std::invalid_argument("native settings extent differs from owned drawable");
    if(!std::isfinite(settings.m_Brightness) || !std::isfinite(settings.m_Contrast) || !std::isfinite(settings.m_MaxDistance))
        throw std::invalid_argument("nonfinite native game settings");
    // Preserve original quality values and requests. Actual caps negotiation,
    // sampler clamping and ApplySettings remain the original engine's work.
    return settings;
}
}
