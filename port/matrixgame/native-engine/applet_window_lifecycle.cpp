// SPDX-License-Identifier: GPL-2.0-or-later
#include "applet_window_lifecycle.hpp"
namespace srhd_awa::matrixgame::applet_window {
void Lifecycle::Attach(){
    if(attached_)throw std::logic_error("native applet window already attached");
    if(restore_result_)throw std::logic_error("previous applet visibility restore failed");
    backend_.Start();attached_=true;known_=false;exit_=false;visible_=baseline_;
    try{Accept(backend_.Read());}catch(...){Detach();throw;}
}
std::uint32_t Lifecycle::Detach()noexcept{
    if(!attached_)return restore_result_;
    // Remove only our hook before calling IPC; borrowed services remain alive.
    backend_.Stop();attached_=false;known_=false;
    if(visible_!=baseline_){restore_result_=backend_.RestoreVisible(baseline_);if(!restore_result_)visible_=baseline_;}
    return restore_result_;
}
void Lifecycle::Accept(Event event){
    if(event.exit){if(!exit_){exit_=true;observer_.Exit();}throw ExitRequested();}
    const bool value=event.focus==Focus::Foreground;
    if(!known_ || value!=foreground_){known_=true;foreground_=value;observer_.FocusChanged(value);}
}
void Lifecycle::Update(){
    if(!attached_)throw std::logic_error("native applet window not attached");
    if(exit_)throw ExitRequested();
    Event event{};
    // Bound work even under a continuous stream. Remaining messages stay queued.
    for(unsigned i=0;i<256 && backend_.Next(event);++i)Accept(event);
}
void Lifecycle::Show(bool visible){
    Update();
    if(visible!=visible_){backend_.SetVisible(visible);visible_=visible;}
}
void Lifecycle::WaitForDrawable(){
    Update();
    if(!visible_)throw std::logic_error("show native window before drawing");
    while(!Drawable()){backend_.Wait();Update();}
}
}
