// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_input_engine.hpp"
#include "3g.hpp"
#include "input.hpp"
namespace srhd_awa::matrixgame::native_input {
namespace {bool Active(){return g_FormCur && (g_Flags&GFLAG_APPACTIVE) && !(g_Flags&GFLAG_APPCLOSE);}}
void EngineSink::Key(bool down,std::uint8_t key){
    if(down)Input::onKeyDown(key);else Input::onKeyUp(key);
    if(Active())g_FormCur->Keyboard(down,key);
}
void EngineSink::Move(int x,int y){if(Active())g_FormCur->MouseMove(x,y);}
void EngineSink::Mouse(Button event,int key,int x,int y){
    if(Active())g_FormCur->MouseKey(event==Button::Down?B_DOWN:event==Button::Up?B_UP:event==Button::Double?B_DOUBLE:B_WHEEL,key,x,y);
}
void EngineSink::ReconcileMouseKeys(const std::bitset<256>& keys,unsigned buttons)noexcept{
    const bool mouse_active=Active();
    for(unsigned i=0;i<3;++i){const std::uint8_t vk=i==0?1:i==1?2:4;
        if(keys[vk] || (mouse_active && (buttons&(1u<<i))))Input::onKeyDown(vk);else Input::onKeyUp(vk);
    }
}
std::bitset<256> ReadEngineKeys(){std::bitset<256> keys;for(unsigned i=0;i<256;++i)keys[i]=Input::isVKeyPressed(std::uint8_t(i));return keys;}
void RestoreEngineKeys(const std::bitset<256>& keys)noexcept{for(unsigned i=0;i<256;++i)if(keys[i])Input::onKeyDown(std::uint8_t(i));else Input::onKeyUp(std::uint8_t(i));}
}
