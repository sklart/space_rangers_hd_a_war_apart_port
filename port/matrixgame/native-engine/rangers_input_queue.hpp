// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_ui_target.hpp"
#include "rangers_canvas_pixels.hpp"
#include <deque>
#include <optional>
#include <algorithm>
namespace srhd_awa::matrixgame {
// Original GI message format, independent of a Win32 HWND or SDL event source.
// Dispatch queued events only after HID polling has completed: a UI callback
// may start FRun, close a modal or replace the active message-loop owner.
class RangersInputQueue final:public native_input::UiTarget {
public:
 struct Event {std::uint32_t message,wparam;std::int32_t lparam;};
 static constexpr unsigned MoveMessage=0x200,KeyDown=0x100,KeyUp=0x101;
 RangersInputQueue(int width,int height,int output_width,int output_height)
  :layout_(MakeLayout(width,height,output_width,output_height)){}
 bool Enabled()const noexcept override{return enabled_;}
 // Ownership transitions discard old queued events and held state. Cancellation
 // of a live original control belongs to the message-loop owner before this call.
 void Enable(bool value)noexcept{Clear();enabled_=value;}
 void Clear()noexcept{events_.clear();keys_.reset();pressed_=0;}
 bool KeyHeld(std::uint8_t key)const noexcept{return keys_[key];}
 std::pair<int,int> Position()const noexcept{return {x_,y_};}
 std::pair<int,int> PhysicalPoint(int x,int y)const{
  if(x<0 || y<0 || x>=layout_.width || y>=layout_.height)throw std::invalid_argument("native UI warp outside canvas");
  return {layout_.x+int((2*std::int64_t(x)+1)*layout_.draw_width/(2*layout_.width)),
          layout_.y+int((2*std::int64_t(y)+1)*layout_.draw_height/(2*layout_.height))};
 }
 void Geometry(int width,int height,int output_width,int output_height){
  auto next=MakeLayout(width,height,output_width,output_height);Clear();layout_=next;x_=y_=-1;
 }
 std::optional<Event> Take(){if(events_.empty())return {};auto e=events_.front();events_.pop_front();return e;}
 std::size_t Pending()const noexcept{return events_.size();}
 void Key(bool down,std::uint8_t key)override{
  keys_.set(key,down);Push({down?KeyDown:KeyUp,key,0});
 }
 void Move(int x,int y)override{Map(x,y);Push({MoveMessage,Modifiers(),Point()});}
 void Mouse(native_input::Button event,int key,int x,int y)override{
  Map(x,y);
  if(event==native_input::Button::Wheel){
   if(Inside()){
    if(key<-32768 || key>32767)throw std::invalid_argument("UI wheel delta outside signed word");
    Push({0x20a,Modifiers()|(std::uint32_t(std::uint16_t(key))<<16),Point()});
   }
   return;
  }
  const unsigned index=key==1?0:key==2?1:key==4?2:3;
  if(index==3)throw std::invalid_argument("UI mouse virtual key");
  const unsigned bit=1u<<index;
  const bool up=event==native_input::Button::Up;
  if(up){if(!(pressed_&bit))return;pressed_&=~bit;}
  else{if(!Inside())return;pressed_|=bit;}
  keys_.set(std::uint8_t(key),!up);
  constexpr unsigned bases[]{0x201,0x204,0x207};
  Push({bases[index]+(up?1:event==native_input::Button::Double?2:0),Modifiers(),Point()});
 }
 void ReconcileMouseKeys(const std::bitset<256>& keys,unsigned)noexcept override{
  // GI reads physical button state, including a double-click's second down.
  // MatrixGame's separate double-click keyboard latch remains in EngineSink.
  for(unsigned i=0;i<3;++i){const unsigned vk=i==0?1:i==1?2:4;keys_.set(vk,keys[vk] || (pressed_&(1u<<i)));}
 }
private:
 rangers_canvas::Plan layout_;std::deque<Event> events_;std::bitset<256> keys_;
 unsigned pressed_{};int x_{-1},y_{-1};bool enabled_{true};
 static rangers_canvas::Plan MakeLayout(int w,int h,int ow,int oh){
  if(w<=0 || w>16384)throw std::invalid_argument("native UI canvas width");
  return rangers_canvas::Layout(w*2,w,h,ow,oh);
 }
 bool Inside()const noexcept{return x_>=0 && y_>=0;}
 void Map(int x,int y)noexcept{
  if(x<layout_.x || y<layout_.y || x>=layout_.x+layout_.draw_width || y>=layout_.y+layout_.draw_height){x_=y_=-1;return;}
  x_=int(std::int64_t(x-layout_.x)*layout_.width/layout_.draw_width);
  y_=int(std::int64_t(y-layout_.y)*layout_.height/layout_.draw_height);
 }
 std::int32_t Point()const noexcept{return std::int32_t(std::uint32_t(std::uint16_t(x_))|(std::uint32_t(std::uint16_t(y_))<<16));}
 unsigned Modifiers()const noexcept{return (pressed_&1?1:0)|(pressed_&2?2:0)|(keys_[16]?4:0)|(keys_[17]?8:0)|(pressed_&4?16:0);}
 void Push(Event e){
  if(e.message==MoveMessage && !events_.empty() && events_.back().message==MoveMessage){events_.back()=e;return;}
  if(events_.size()>=2048)throw std::runtime_error("native Rangers UI event queue overflow");
  events_.push_back(e);
 }
};
}
