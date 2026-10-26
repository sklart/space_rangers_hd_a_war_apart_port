// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_input.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
namespace srhd_awa::matrixgame::native_input {
void Router::Extent(unsigned w,unsigned h){if(!w || !h || w>32768 || h>32768)throw std::invalid_argument("native input extent");width_=w;height_=h;x_=std::clamp(x_,0,int(w)-1);y_=std::clamp(y_,0,int(h)-1);}
void Router::Reset()noexcept{sources_={};keys_.reset();buttons_=mouse_keys_=0;clicks_={};repeat_={};active_=false;sink_.ReconcileMouseKeys(keys_,0);}
void Router::Focus(bool active){
    if(active_==active)return;
    if(!active){
        // Release held logical input before deactivation; never leave drag/keys stuck.
        for(unsigned k=0;k<256;++k)if(keys_[k])sink_.Key(false,std::uint8_t(k));
        for(unsigned i=0;i<3;++i)if(buttons_&(1u<<i))sink_.Mouse(Button::Up,i==0?1:i==1?2:4,x_,y_);
        Reset();
    }
    active_=active;
}
void Router::Feed(Source source,const Sample& sample,std::uint64_t time){
    if(unsigned(source)>=sources_.size() || (sample.buttons&~7u))throw std::invalid_argument("native input source/buttons");
    if(!active_)return;
    sources_[unsigned(source)]=sample;
    if(sample.point){const int x=std::clamp(sample.x,0,int(width_)-1),y=std::clamp(sample.y,0,int(height_)-1);
        if(x!=x_ || y!=y_){x_=x;y_=y;sink_.Move(x_,y_);}}
    std::bitset<256> keys;unsigned buttons{};
    for(const auto& item:sources_){keys|=item.keys;buttons|=item.buttons;}
    for(unsigned k=0;k<256;++k)if(keys[k]!=keys_[k]){keys_[k]=keys[k];repeat_[k]=time+500;sink_.Key(keys[k],std::uint8_t(k));}
    for(unsigned i=0;i<3;++i)if((buttons^buttons_)&(1u<<i)){
        const bool down=buttons&(1u<<i);const int vk=i==0?1:i==1?2:4;
        if(down){auto& click=clicks_[i];const bool twice=click.valid && time>=click.time && time-click.time<=500 && std::abs(x_-click.x)<=4 && std::abs(y_-click.y)<=4;
            // Original B_DOUBLE invokes its action without Input::onKeyDown.
            // Preserve that latch behaviour, including when aliases overlap.
            if(!twice)mouse_keys_|=1u<<i;
            click={ !twice,time,x_,y_};buttons_|=1u<<i;sink_.Mouse(twice?Button::Double:Button::Down,vk,x_,y_);
        }else{buttons_&=~(1u<<i);mouse_keys_&=~(1u<<i);sink_.Mouse(Button::Up,vk,x_,y_);}}
    if(sample.wheel)sink_.Mouse(Button::Wheel,sample.wheel,x_,y_);
    sink_.ReconcileMouseKeys(keys_,mouse_keys_);
}
void Router::Repeat(std::uint64_t time){
    if(!active_)return;
    for(unsigned k=0;k<256;++k)if(keys_[k] && k!=16 && k!=17 && k!=18 && k!=91 && k!=92 && time>=repeat_[k]){
        // Native typematic defaults: 500 ms / 30 Hz; bound catch-up after a stall.
        unsigned count=0;while(time>=repeat_[k] && count++<4){repeat_[k]+=33;sink_.Key(true,std::uint8_t(k));}
        if(time>=repeat_[k])repeat_[k]=time+33;
    }
    sink_.ReconcileMouseKeys(keys_,mouse_keys_);
}
std::uint8_t HidKeyToVirtual(unsigned key)noexcept{
    if(key>=4 && key<=29)return std::uint8_t('A'+key-4);
    if(key>=30 && key<=38)return std::uint8_t('1'+key-30);
    if(key==39)return '0';
    if(key>=58 && key<=69)return std::uint8_t(0x70+key-58);
    if(key>=89 && key<=97)return std::uint8_t(0x61+key-89);
    switch(key){case 40:return 13;case 41:return 27;case 42:return 8;case 43:return 9;case 44:return 32;
    case 45:return 0xbd;case 46:return 0xbb;case 47:return 0xdb;case 48:return 0xdd;case 49:return 0xdc;
    case 51:return 0xba;case 52:return 0xde;case 53:return 0xc0;case 54:return 0xbc;case 55:return 0xbe;case 56:return 0xbf;
    case 57:return 20;case 70:return 44;case 71:return 145;case 72:return 19;
    case 73:return 45;case 74:return 36;case 75:return 33;case 76:return 46;case 77:return 35;case 78:return 34;
    case 79:return 39;case 80:return 37;case 81:return 40;case 82:return 38;case 83:return 144;
    case 84:return 111;case 85:return 106;case 86:return 109;case 87:return 107;case 88:return 13;case 98:return 96;case 99:return 110;
    case 224:case 228:return 17;case 225:case 229:return 16;case 226:case 230:return 18;
    case 227:return 91;case 231:return 92;default:return 0;}
}
}
