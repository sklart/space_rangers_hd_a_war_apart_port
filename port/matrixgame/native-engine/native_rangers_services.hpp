// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "MatrixGameDll.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
// Function pointers are copied; their host/context remains borrowed and must
// outlive original Deinit, including sound/text resource destruction callbacks.
class NativeRangersServicesScope {
    SMGDRangersInterface table_;
public:
    explicit NativeRangersServicesScope(const SMGDRangersInterface& input):table_(input){
#define REQUIRE_SERVICE(name) if(!table_.name)throw std::invalid_argument("missing native Rangers service: " #name)
        REQUIRE_SERVICE(m_Sound);REQUIRE_SERVICE(m_SoundCreate);REQUIRE_SERVICE(m_SoundDestroy);
        REQUIRE_SERVICE(m_SoundPlay);REQUIRE_SERVICE(m_SoundIsPlay);REQUIRE_SERVICE(m_SoundVolume);
        REQUIRE_SERVICE(m_SoundPan);REQUIRE_SERVICE(m_SoundGetVolume);REQUIRE_SERVICE(m_SoundGetPan);
        REQUIRE_SERVICE(m_RangersText);REQUIRE_SERVICE(m_RangersTextClear);REQUIRE_SERVICE(m_ProgressBar);
        REQUIRE_SERVICE(m_Takt);REQUIRE_SERVICE(m_Begin);REQUIRE_SERVICE(m_MusicVolumeGet);REQUIRE_SERVICE(m_MusicVolumeSet);
#undef REQUIRE_SERVICE
        if(g_RangersInterface)throw std::logic_error("native Rangers services already bound");
        g_RangersInterface=&table_;
    }
    ~NativeRangersServicesScope(){if(g_RangersInterface!=&table_)std::terminate();g_RangersInterface=nullptr;}
    NativeRangersServicesScope(const NativeRangersServicesScope&)=delete;
    NativeRangersServicesScope& operator=(const NativeRangersServicesScope&)=delete;
};
}
