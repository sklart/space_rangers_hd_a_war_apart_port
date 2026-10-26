// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>
#include <array>
namespace srhd_awa::matrixgame::native_cursor {
struct Icon {unsigned width,height,hot_x,hot_y;std::vector<std::byte> rgba;};
struct Vertex {float x,y,u,v;};
class Store {
public:
    struct State {std::uintptr_t selected{};int display_count{};bool device_visible{};int x{},y{};};
    std::uintptr_t Create(unsigned,unsigned,unsigned,unsigned,std::span<const std::byte>);
    std::uintptr_t Exchange(std::uintptr_t);
    void Destroy(std::uintptr_t);
    int Show(bool);
    bool ShowDevice(bool)noexcept;
    void Position(int x,int y)noexcept{state_.x=x;state_.y=y;}
    State Read()const noexcept{return state_;}
    void Restore(State);
    std::shared_ptr<const Icon> Visible()const;
    std::size_t Count()const noexcept{return icons_.size();}
private:
    std::map<std::uintptr_t,std::shared_ptr<Icon>> icons_;
    State state_{};
};
class Provider {
public:virtual ~Provider()=default;virtual Store& CursorStore()=0;
};
std::array<Vertex,4> Quad(const Icon&,Store::State);
std::vector<std::byte> Bgra(const Icon&);
}
