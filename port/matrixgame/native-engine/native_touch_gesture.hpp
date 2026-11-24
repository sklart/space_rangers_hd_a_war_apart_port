// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstdlib>
namespace srhd_awa::matrixgame::native_input {
struct TouchPanDirections{bool left{},right{},up{},down{};};
inline TouchPanDirections PanToCamera(int dx,int dy)noexcept{return {dx>0,dx<0,dy>0,dy<0};}
// Single-finger battle gesture: a tap clicks on release; a drag pans instead.
class TouchGesture {
    bool active_{},dragging_{};int start_x_{},start_y_{},last_x_{},last_y_{};
public:
    static constexpr int DragThreshold=12;
    struct Motion{int dx{},dy{};};
    void Begin(int x,int y)noexcept{active_=true;dragging_=false;start_x_=last_x_=x;start_y_=last_y_=y;}
    Motion Move(int x,int y)noexcept{
        if(!active_)return {};
        const auto total_x=x-start_x_,total_y=y-start_y_;
        if(!dragging_ && (std::abs(total_x)>=DragThreshold || std::abs(total_y)>=DragThreshold)){
            dragging_=true;last_x_=start_x_;last_y_=start_y_;
        }
        const Motion motion=dragging_?Motion{x-last_x_,y-last_y_}:Motion{};
        last_x_=x;last_y_=y;return motion;
    }
    bool End()noexcept{const bool tap=active_ && !dragging_;active_=dragging_=false;return tap;}
    void Cancel()noexcept{active_=dragging_=false;}
};
class ZoomRepeat {
    int direction_{};std::uint64_t last_{};
public:
    static constexpr std::uint64_t RepeatMs=220;
    int Update(bool zoom_in,bool zoom_out,std::uint64_t now)noexcept{
        const int direction=int(zoom_in)-int(zoom_out);
        if(!direction){direction_=0;last_=0;return 0;}
        if(direction!=direction_ || now<last_){
            direction_=direction;last_=now;return direction;
        }
        if(now-last_>=RepeatMs){last_=now;return direction;}
        return 0;
    }
    void Reset()noexcept{direction_=0;last_=0;}
};
}
