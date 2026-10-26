// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_input.hpp"
namespace srhd_awa::matrixgame::native_input {
class EngineSink final:public Sink {
public:
    void Key(bool,std::uint8_t)override;
    void Move(int,int)override;
    void Mouse(Button,int,int,int)override;
    void ReconcileMouseKeys(const std::bitset<256>&,unsigned)noexcept override;
};
std::bitset<256> ReadEngineKeys();
void RestoreEngineKeys(const std::bitset<256>&)noexcept;
}
