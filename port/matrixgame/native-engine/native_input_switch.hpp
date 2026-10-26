// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_input_engine.hpp"
#include "native_cursor.hpp"
#include <switch.h>
namespace srhd_awa::matrixgame::native_input {
class SwitchInput {
public:
    explicit SwitchInput(native_cursor::Store& store):router_(sink_),cursor_(store){}
    void Attach();void Detach()noexcept;
    void Focus(bool);
    void Poll(unsigned width,unsigned height,bool active);
    void ConstrainToExtent(unsigned width,unsigned height);
private:
    EngineSink sink_;Router router_;native_cursor::Store& cursor_;
    PadState pad_{};std::bitset<256> saved_keys_;bool attached_{};
    std::uint64_t keyboard_sample_{},mouse_sample_{},touch_sample_{},last_ms_{};
    bool keyboard_known_{},mouse_known_{},touch_known_{};
    float pointer_x_{},pointer_y_{};std::uint32_t touch_id_{};bool touching_{};
    void Prime();
};
}
