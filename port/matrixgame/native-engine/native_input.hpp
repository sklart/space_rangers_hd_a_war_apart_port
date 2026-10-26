// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <bitset>
#include <cstdint>
namespace srhd_awa::matrixgame::native_input {
enum class Source:unsigned {Keyboard,Mouse,Touch,Pad,Count};
enum class Button {Down,Up,Double,Wheel};
struct Sample {std::bitset<256> keys;unsigned buttons{};bool point{};int x{},y{},wheel{};};
class Sink {
public:virtual ~Sink()=default;
    virtual void Key(bool,std::uint8_t)=0;
    virtual void Move(int,int)=0;
    virtual void Mouse(Button,int,int,int)=0;
    // Original MouseKey and Keyboard share VK_L/R/MBUTTON in Input. Reconcile
    // their owners after callbacks, without generating extra game events.
    virtual void ReconcileMouseKeys(const std::bitset<256>&,unsigned)noexcept=0;
};
// Merge independent HID sources; a release by one source cannot release another.
class Router {
public:
    explicit Router(Sink& sink):sink_(sink){}
    void Extent(unsigned,unsigned);
    void Feed(Source,const Sample&,std::uint64_t milliseconds);
    void Focus(bool);
    void Repeat(std::uint64_t milliseconds);
    void Reset()noexcept;
    int X()const noexcept{return x_;}int Y()const noexcept{return y_;}
    bool Active()const noexcept{return active_;}
private:
    Sink& sink_;std::array<Sample,unsigned(Source::Count)> sources_{};
    std::bitset<256> keys_;unsigned buttons_{},mouse_keys_{},width_{1},height_{1};int x_{},y_{};bool active_{};
    struct Click {bool valid{};std::uint64_t time{};int x{},y{};};std::array<Click,3> clicks_{};
    std::array<std::uint64_t,256> repeat_{};
};
std::uint8_t HidKeyToVirtual(unsigned)noexcept;
}
