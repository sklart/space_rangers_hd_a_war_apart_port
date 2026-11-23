// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include <cstdio>
#include <stdexcept>

namespace srhd_awa::matrixgame {
// Process-start operation before any game workers. Retain earlier logs and
// explicitly assign the original writer to the selected user-root path.
inline void InitializeRangersLog(const char* path) {
    if(!path || !*path)throw std::invalid_argument("game log path missing");
    auto* file=std::fopen(path,"ab");
    if(!file)throw std::runtime_error("cannot open game log");
    if(std::fclose(file)!=0)throw std::runtime_error("cannot close game log");
    pas::text_assign(GR_Main::SessionLog,path,false);
    GR_Main::AppendLogLineThreadSafe("integrated application: startup"_a);
}
}
