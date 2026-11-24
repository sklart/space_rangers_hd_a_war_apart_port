// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_input_switch.hpp"
#include "native_ui_target.hpp"
#include "native_stick_policy.hpp"
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
Sample Keyboard(const HidKeyboardState& value){Sample sample;
    for(unsigned key=0;key<256;++key)if(hidKeyboardStateGetKey(&value,HidKeyboardKey(key))){const auto vk=HidKeyToVirtual(key);if(vk)sample.keys.set(vk);}
    if(value.modifiers&HidKeyboardModifier_Control)sample.keys.set(17);
    if(value.modifiers&HidKeyboardModifier_Shift)sample.keys.set(16);
    if(value.modifiers&(HidKeyboardModifier_LeftAlt|HidKeyboardModifier_RightAlt))sample.keys.set(18);
    return sample;
}
}
void SwitchInput::Attach(){
    if(attached_)throw std::logic_error("native input already attached");
    if(!serviceIsActive(hidGetServiceSession()) || !hidGetSharedmemAddr())throw platform3g::Unsupported("live libnx HID service/shared memory required");
    saved_keys_=ReadEngineKeys();
    // Idempotent libnx peripheral activation; no service shutdown or Npad policy change.
    hidInitializeMouse();hidInitializeKeyboard();hidInitializeTouchScreen();padInitializeDefault(&pad_);
    router_.Reset();keyboard_known_=mouse_known_=touch_known_=touching_=false;touch_gesture_.Cancel();zoom_repeat_.Reset();
    last_ms_=Now();pointer_x_=float(cursor_.Read().x);pointer_y_=float(cursor_.Read().y);restore_position_=true;attached_=true;
}
void SwitchInput::Detach()noexcept{if(attached_){router_.Reset();touch_gesture_.Cancel();zoom_repeat_.Reset();RestoreEngineKeys(saved_keys_);attached_=false;}}
void SwitchInput::Prime(){
    HidMouseState mouse{};blocked_mouse_.Prime(0);if(hidGetMouseStates(&mouse,1)){mouse_sample_=mouse.sampling_number;mouse_known_=true;blocked_mouse_.Prime(mouse.buttons&7);}
    HidKeyboardState keyboard{};blocked_keyboard_.Prime({});if(hidGetKeyboardStates(&keyboard,1)){keyboard_sample_=keyboard.sampling_number;keyboard_known_=true;blocked_keyboard_.Prime(Keyboard(keyboard).keys);}
    HidTouchScreenState touch{};blocked_touch_.Prime(0);if(hidGetTouchScreenStates(&touch,1)){touch_sample_=touch.sampling_number;touch_known_=true;blocked_touch_.Prime(touch.count>0?1:0);}
    padUpdate(&pad_);blocked_pad_.Prime(padGetButtons(&pad_));
    touching_=false;battle_touch_=false;touch_wait_release_=false;touch_gesture_.Cancel();zoom_repeat_.Reset();last_ms_=Now();pointer_x_=float(router_.X());pointer_y_=float(router_.Y());
}
void SwitchInput::Focus(bool active){if(attached_ && router_.Active()!=active){Prime();router_.Focus(active);}}
void SwitchInput::CancelHeldInput(){
    if(!attached_)throw std::logic_error("native input reset requires an attached window");
    const bool active=router_.Active();router_.Reset();Prime();router_.Focus(active);
}
void SwitchInput::ConstrainToExtent(unsigned width,unsigned height){
    if(!attached_)throw std::logic_error("native cursor constraint requires attached input");
    const auto x=router_.X(),y=router_.Y();router_.Extent(width,height);
    pointer_x_=float(router_.X());pointer_y_=float(router_.Y());cursor_.Position(router_.X(),router_.Y());
    if(x!=router_.X() || y!=router_.Y())sink_.Move(router_.X(),router_.Y());
}
void SwitchInput::Warp(int x,int y,unsigned width,unsigned height){
    if(!attached_)throw std::logic_error("native cursor warp requires attached input");
    router_.Extent(width,height);router_.Warp(x,y);pointer_x_=float(router_.X());pointer_y_=float(router_.Y());cursor_.Position(router_.X(),router_.Y());restore_position_=false;
}
void SwitchInput::Poll(unsigned width,unsigned height,bool active){
    if(!attached_)throw std::logic_error("native input not attached");
    router_.Extent(width,height);
    if(restore_position_){const auto position=cursor_.Read();router_.Warp(position.x,position.y);restore_position_=false;}
    Focus(active);const auto now=Now();
    const auto elapsed=std::min<std::uint64_t>(now-last_ms_,100);last_ms_=now;
    HidMouseState mouse[17];const auto mice=hidGetMouseStates(mouse,17);
    Samples(mouse,mice,mouse_sample_,mouse_known_,[&](const HidMouseState& value){Sample sample;
        if(value.attributes&HidMouseAttribute_IsConnected){sample.buttons=blocked_mouse_.Filter(value.buttons&7);sample.point=value.delta_x || value.delta_y;
            sample.x=int(std::clamp<std::int64_t>(std::int64_t(router_.X())+value.delta_x,0,width-1));
            sample.y=int(std::clamp<std::int64_t>(std::int64_t(router_.Y())+value.delta_y,0,height-1));sample.wheel=value.wheel_delta_y;}
        router_.Feed(Source::Mouse,sample,now);});
    if(!mice)router_.Feed(Source::Mouse,{},now);
    HidKeyboardState keyboard[17];const auto keyboards=hidGetKeyboardStates(keyboard,17);
    Samples(keyboard,keyboards,keyboard_sample_,keyboard_known_,[&](const HidKeyboardState& value){auto sample=Keyboard(value);
        sample.keys=blocked_keyboard_.Filter(sample.keys);
        router_.Feed(Source::Keyboard,sample,now);});
    if(!keyboards)router_.Feed(Source::Keyboard,{},now);
    HidTouchScreenState touch[17];const auto touches=hidGetTouchScreenStates(touch,17);
    int pan_x=0,pan_y=0;bool battle_touch_output=battle_touch_;
    auto coordinate=[](std::uint32_t value,unsigned extent,unsigned source){return int(std::uint64_t(std::min(value,source-1))*extent/source);};
    Samples(touch,touches,touch_sample_,touch_known_,[&](const HidTouchScreenState& value){Sample sample;
        if(value.count<0 || value.count>16)throw std::runtime_error("invalid native touch count");
        if(value.count && !blocked_touch_.Filter(1))return;
        if(!value.count)blocked_touch_.Filter(0);
        if(!value.count)touch_wait_release_=false;
        else if(touch_wait_release_)return;
        if(!value.count && touching_){
            if(battle_touch_){
                battle_touch_output=true;
                if(touch_gesture_.End()){Sample tap;tap.point=true;tap.x=touch_x_;tap.y=touch_y_;tap.buttons=1;
                    router_.Feed(Source::Touch,tap,now);router_.Feed(Source::Touch,{},now);}
            }
            touching_=false;
        }
        if(!touching_ && value.count){touching_=true;touch_id_=value.touches[0].finger_id;battle_touch_=!CurrentUiTarget();battle_touch_output=battle_touch_;
            touch_x_=coordinate(value.touches[0].x,width,1280);touch_y_=coordinate(value.touches[0].y,height,720);
            if(battle_touch_){touch_gesture_.Begin(touch_x_,touch_y_);battle_touch_output=true;}}
        bool found=false;for(unsigned i=0;i<value.count && i<16;++i)if(value.touches[i].finger_id==touch_id_ && touching_){found=true;
            touch_x_=coordinate(value.touches[i].x,width,1280);touch_y_=coordinate(value.touches[i].y,height,720);
            if(battle_touch_){const auto motion=touch_gesture_.Move(touch_x_,touch_y_);pan_x+=motion.dx;pan_y+=motion.dy;battle_touch_output=true;}
            else{sample.point=true;sample.x=touch_x_;sample.y=touch_y_;sample.buttons=1;}break;}
        if(!found && value.count && touching_){touching_=false;touch_wait_release_=true;touch_gesture_.Cancel();}
        if(!battle_touch_ || (!found && value.count))router_.Feed(Source::Touch,sample,now);});
    if(!touches){touching_=false;touch_gesture_.Cancel();router_.Feed(Source::Touch,{},now);battle_touch_output=false;}
    if(battle_touch_output){Sample swipe;
        auto scroll=[&](bool condition,EKeyAction action){if(condition){const auto key=g_Config.m_KeyActions[action];if(key)swipe.keys.set(key);}};
        const auto directions=PanToCamera(pan_x,pan_y);
        scroll(directions.left,KA_SCROLL_LEFT);scroll(directions.right,KA_SCROLL_RIGHT);
        scroll(directions.up,KA_SCROLL_UP);scroll(directions.down,KA_SCROLL_DOWN);
        router_.Feed(Source::Touch,swipe,now);
    }
    padUpdate(&pad_);Sample controller;const auto physical_buttons=padGetButtons(&pad_);const auto buttons=blocked_pad_.Filter(physical_buttons);
    controller.buttons=((buttons&HidNpadButton_A)?1u:0u)|((buttons&HidNpadButton_B)?2u:0u);
    const auto left_stick=padGetStickPos(&pad_,0);
    const auto right_stick=padGetStickPos(&pad_,1);
    const auto battle_sticks=MapBattleSticks({left_stick.x,left_stick.y},{right_stick.x,right_stick.y});
    if(CurrentUiTarget()){
        const std::pair<std::uint64_t,unsigned> ui_keys[]={{HidNpadButton_Left,37},{HidNpadButton_Right,39},{HidNpadButton_Up,38},{HidNpadButton_Down,40},
            {HidNpadButton_L,33},{HidNpadButton_R,34},{HidNpadButton_X,13},{HidNpadButton_Y,32},{HidNpadButton_Minus,9},{HidNpadButton_ZL,16},{HidNpadButton_ZR,17}};
        for(auto [mask,key]:ui_keys)if(buttons&mask)controller.keys.set(key);
    }else{
    auto bind=[&](std::uint64_t mask,EKeyAction action){if(buttons&mask){const auto key=g_Config.m_KeyActions[action];if(key)controller.keys.set(key);}};
    bind(HidNpadButton_Left,KA_SCROLL_LEFT);bind(HidNpadButton_Right,KA_SCROLL_RIGHT);
    bind(HidNpadButton_Up,KA_SCROLL_UP);bind(HidNpadButton_Down,KA_SCROLL_DOWN);
    bind(HidNpadButton_ZL,KA_SHIFT);bind(HidNpadButton_ZR,KA_FIRE);
    bind(HidNpadButton_L,KA_ROTATE_LEFT);bind(HidNpadButton_R,KA_ROTATE_RIGHT);
    bind(HidNpadButton_X,KA_UNIT_ENTER);bind(HidNpadButton_Y,KA_ORDER_CANCEL);
    const auto scroll=battle_sticks.camera;
    auto action=[&](bool held,EKeyAction value){if(held){const auto key=g_Config.m_KeyActions[value];if(key)controller.keys.set(key);}};
    action(scroll.left,KA_SCROLL_LEFT);action(scroll.right,KA_SCROLL_RIGHT);
    action(scroll.up,KA_SCROLL_UP);action(scroll.down,KA_SCROLL_DOWN);
    controller.wheel=zoom_repeat_.Update(buttons&HidNpadButton_StickR,buttons&HidNpadButton_StickL,now);
    }
    if(CurrentUiTarget())zoom_repeat_.Reset();
    if(buttons&HidNpadButton_Plus)controller.keys.set(27);
    // The main Rangers port uses the left stick for its UI pointer. Keep the
    // right stick available too, and retain its separate battle mapping.
    StickPoint stick=battle_sticks.pointer;
    if(CurrentUiTarget()){
        if(std::abs(stick.x)<=5000 && std::abs(stick.y)<=5000)stick={right_stick.x,right_stick.y};
    }
    auto speed=[](int value){const auto magnitude=std::abs(value);return magnitude<=5000?0.f:std::copysign(float(magnitude-5000)/27767.f,float(value));};
    if(int(pointer_x_)!=router_.X() || int(pointer_y_)!=router_.Y()){pointer_x_=float(router_.X());pointer_y_=float(router_.Y());}
    if(speed(stick.x)!=0 || speed(stick.y)!=0){
        pointer_x_=std::clamp(pointer_x_+speed(stick.x)*float(elapsed)*0.8f,0.f,float(width-1));
        pointer_y_=std::clamp(pointer_y_-speed(stick.y)*float(elapsed)*0.8f,0.f,float(height-1));
        controller.point=true;controller.x=int(pointer_x_);controller.y=int(pointer_y_);}
    router_.Feed(Source::Pad,controller,now);router_.Repeat(now);cursor_.Position(router_.X(),router_.Y());
}
}
