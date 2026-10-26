// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_input_switch.hpp"
#include "platform_3g.hpp"
#include "MatrixConfig.hpp"
#include <algorithm>
#include <cmath>
namespace srhd_awa::matrixgame::native_input {
namespace {
std::uint64_t Now(){return armTicksToNs(armGetSystemTick())/1'000'000;}
template<class T,class F>void Samples(T* values,std::size_t count,std::uint64_t& previous,bool& known,F consume){
    for(std::size_t i=count;i>0;--i){const auto& value=values[i-1];if(known && value.sampling_number<=previous)continue;
        consume(value);previous=value.sampling_number;known=true;}
}
}
void SwitchInput::Attach(){
    if(attached_)throw std::logic_error("native input already attached");
    if(!serviceIsActive(hidGetServiceSession()) || !hidGetSharedmemAddr())throw platform3g::Unsupported("live libnx HID service/shared memory required");
    saved_keys_=ReadEngineKeys();
    // Idempotent libnx peripheral activation; no service shutdown or Npad policy change.
    hidInitializeMouse();hidInitializeKeyboard();hidInitializeTouchScreen();padInitializeDefault(&pad_);
    router_.Reset();keyboard_known_=mouse_known_=touch_known_=touching_=false;
    last_ms_=Now();pointer_x_=float(cursor_.Read().x);pointer_y_=float(cursor_.Read().y);attached_=true;
}
void SwitchInput::Detach()noexcept{if(attached_){router_.Reset();RestoreEngineKeys(saved_keys_);attached_=false;}}
void SwitchInput::Prime(){
    HidMouseState mouse{};if(hidGetMouseStates(&mouse,1)){mouse_sample_=mouse.sampling_number;mouse_known_=true;}
    HidKeyboardState keyboard{};if(hidGetKeyboardStates(&keyboard,1)){keyboard_sample_=keyboard.sampling_number;keyboard_known_=true;}
    HidTouchScreenState touch{};if(hidGetTouchScreenStates(&touch,1)){touch_sample_=touch.sampling_number;touch_known_=true;}
    touching_=false;last_ms_=Now();pointer_x_=float(router_.X());pointer_y_=float(router_.Y());
}
void SwitchInput::Focus(bool active){if(attached_ && router_.Active()!=active){Prime();router_.Focus(active);}}
void SwitchInput::ConstrainToExtent(unsigned width,unsigned height){
    if(!attached_)throw std::logic_error("native cursor constraint requires attached input");
    const auto x=router_.X(),y=router_.Y();router_.Extent(width,height);
    pointer_x_=float(router_.X());pointer_y_=float(router_.Y());cursor_.Position(router_.X(),router_.Y());
    if(x!=router_.X() || y!=router_.Y())sink_.Move(router_.X(),router_.Y());
}
void SwitchInput::Poll(unsigned width,unsigned height,bool active){
    if(!attached_)throw std::logic_error("native input not attached");
    router_.Extent(width,height);Focus(active);const auto now=Now();
    const auto elapsed=std::min<std::uint64_t>(now-last_ms_,100);last_ms_=now;
    HidMouseState mouse[17];const auto mice=hidGetMouseStates(mouse,17);
    Samples(mouse,mice,mouse_sample_,mouse_known_,[&](const HidMouseState& value){Sample sample;
        if(value.attributes&HidMouseAttribute_IsConnected){sample.buttons=value.buttons&7;sample.point=value.delta_x || value.delta_y;
            sample.x=int(std::clamp<std::int64_t>(std::int64_t(router_.X())+value.delta_x,0,width-1));
            sample.y=int(std::clamp<std::int64_t>(std::int64_t(router_.Y())+value.delta_y,0,height-1));sample.wheel=value.wheel_delta_y;}
        router_.Feed(Source::Mouse,sample,now);});
    if(!mice)router_.Feed(Source::Mouse,{},now);
    HidKeyboardState keyboard[17];const auto keyboards=hidGetKeyboardStates(keyboard,17);
    Samples(keyboard,keyboards,keyboard_sample_,keyboard_known_,[&](const HidKeyboardState& value){Sample sample;
        for(unsigned key=0;key<256;++key)if(hidKeyboardStateGetKey(&value,HidKeyboardKey(key))){const auto vk=HidKeyToVirtual(key);if(vk)sample.keys.set(vk);}
        if(value.modifiers&HidKeyboardModifier_Control)sample.keys.set(17);
        if(value.modifiers&HidKeyboardModifier_Shift)sample.keys.set(16);
        if(value.modifiers&(HidKeyboardModifier_LeftAlt|HidKeyboardModifier_RightAlt))sample.keys.set(18);
        router_.Feed(Source::Keyboard,sample,now);});
    if(!keyboards)router_.Feed(Source::Keyboard,{},now);
    HidTouchScreenState touch[17];const auto touches=hidGetTouchScreenStates(touch,17);
    Samples(touch,touches,touch_sample_,touch_known_,[&](const HidTouchScreenState& value){Sample sample;
        if(value.count<0 || value.count>16)throw std::runtime_error("invalid native touch count");
        if(!value.count)touching_=false;
        if(!touching_ && value.count){touching_=true;touch_id_=value.touches[0].finger_id;}
        bool found=false;for(unsigned i=0;i<value.count && i<16;++i)if(value.touches[i].finger_id==touch_id_ && touching_){found=true;
            sample.point=true;sample.x=int(std::uint64_t(value.touches[i].x)*width/1280);sample.y=int(std::uint64_t(value.touches[i].y)*height/720);sample.buttons=1;break;}
        if(!found)touching_=false;
        router_.Feed(Source::Touch,sample,now);});
    if(!touches){touching_=false;router_.Feed(Source::Touch,{},now);}
    padUpdate(&pad_);Sample controller;const auto buttons=padGetButtons(&pad_);
    controller.buttons=((buttons&HidNpadButton_A)?1u:0u)|((buttons&HidNpadButton_B)?2u:0u);
    auto bind=[&](std::uint64_t mask,EKeyAction action){if(buttons&mask){const auto key=g_Config.m_KeyActions[action];if(key)controller.keys.set(key);}};
    bind(HidNpadButton_Left,KA_SCROLL_LEFT);bind(HidNpadButton_Right,KA_SCROLL_RIGHT);
    bind(HidNpadButton_Up,KA_SCROLL_UP);bind(HidNpadButton_Down,KA_SCROLL_DOWN);
    bind(HidNpadButton_ZL,KA_SHIFT);bind(HidNpadButton_ZR,KA_FIRE);
    bind(HidNpadButton_L,KA_ROTATE_LEFT);bind(HidNpadButton_R,KA_ROTATE_RIGHT);
    bind(HidNpadButton_X,KA_UNIT_ENTER);bind(HidNpadButton_Y,KA_ORDER_CANCEL);
    const auto left=padGetStickPos(&pad_,0);
    auto action=[&](bool held,EKeyAction value){if(held){const auto key=g_Config.m_KeyActions[value];if(key)controller.keys.set(key);}};
    action(left.x < -12000,KA_UNIT_LEFT);action(left.x > 12000,KA_UNIT_RIGHT);
    action(left.y > 12000,KA_UNIT_FORWARD);action(left.y < -12000,KA_UNIT_BACKWARD);
    if(buttons&HidNpadButton_Plus)controller.keys.set(27);
    const auto stick=padGetStickPos(&pad_,1);
    auto speed=[](int value){const auto magnitude=std::abs(value);return magnitude<=5000?0.f:std::copysign(float(magnitude-5000)/27767.f,float(value));};
    if(int(pointer_x_)!=router_.X() || int(pointer_y_)!=router_.Y()){pointer_x_=float(router_.X());pointer_y_=float(router_.Y());}
    if(speed(stick.x)!=0 || speed(stick.y)!=0){
        pointer_x_=std::clamp(pointer_x_+speed(stick.x)*float(elapsed)*0.8f,0.f,float(width-1));
        pointer_y_=std::clamp(pointer_y_-speed(stick.y)*float(elapsed)*0.8f,0.f,float(height-1));
        controller.point=true;controller.x=int(pointer_x_);controller.y=int(pointer_y_);}
    router_.Feed(Source::Pad,controller,now);router_.Repeat(now);cursor_.Position(router_.X(),router_.Y());
}
}
