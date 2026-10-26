// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_cursor.hpp"
#include <atomic>
#include <limits>
#include <stdexcept>
namespace srhd_awa::matrixgame::native_cursor {
namespace {std::atomic<std::uintptr_t> next{1};}
std::uintptr_t Store::Create(unsigned w,unsigned h,unsigned x,unsigned y,std::span<const std::byte> bytes){
    if(!w || !h || w>1024 || h>1024 || x>=w || y>=h || bytes.size()!=std::size_t(w)*h*4)
        throw std::invalid_argument("native cursor image extent/hotspot/payload");
    auto icon=std::make_shared<Icon>(Icon{w,h,x,y,{bytes.begin(),bytes.end()}});
    const auto id=next.fetch_add(1);if(!id || id==std::numeric_limits<std::uintptr_t>::max())std::terminate();
    icons_.emplace(id,std::move(icon));return id;
}
std::uintptr_t Store::Exchange(std::uintptr_t id){
    if(id && !icons_.contains(id))throw std::invalid_argument("foreign or destroyed native cursor");
    const auto previous=state_.selected;state_.selected=id;return previous;
}
void Store::Destroy(std::uintptr_t id){
    if(!id)return;
    if(state_.selected==id)throw std::logic_error("restore cursor before destroying selected icon");
    if(!icons_.erase(id))throw std::invalid_argument("foreign or destroyed native cursor");
}
int Store::Show(bool value){
    if((value && state_.display_count==std::numeric_limits<int>::max()) || (!value && state_.display_count==std::numeric_limits<int>::min()))
        throw std::overflow_error("native cursor display counter");
    return state_.display_count+=value?1:-1;
}
bool Store::ShowDevice(bool value)noexcept{const auto previous=state_.device_visible;state_.device_visible=value;return previous;}
void Store::Restore(State value){if(value.selected && !icons_.contains(value.selected))throw std::logic_error("native cursor baseline destroyed");state_=value;}
std::shared_ptr<const Icon> Store::Visible()const{
    if(state_.display_count<0 || !state_.selected)return {};
    return icons_.at(state_.selected);
}
std::array<Vertex,4> Quad(const Icon& icon,Store::State position){
    const float x=float(position.x)-float(icon.hot_x)-0.5f,y=float(position.y)-float(icon.hot_y)-0.5f;
    return {{{x,y+icon.height,0,1},{x,y,0,0},{x+icon.width,y+icon.height,1,1},{x+icon.width,y,1,0}}};
}
std::vector<std::byte> Bgra(const Icon& icon){
    if(icon.rgba.size()!=std::size_t(icon.width)*icon.height*4)throw std::invalid_argument("native cursor RGBA payload");
    auto bytes=icon.rgba;for(std::size_t i=0;i<bytes.size();i+=4)std::swap(bytes[i],bytes[i+2]);return bytes;
}
}
