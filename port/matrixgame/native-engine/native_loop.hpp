// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
class CForm;
// Original L3GRun, with only platform message/yield/frame/clock boundaries.
int RunOriginalNativeLoop();
namespace srhd_awa::matrixgame::native_loop {
using Clock=std::chrono::steady_clock;
class Backend {
public:
    virtual ~Backend()=default;
    virtual void Pump()=0;
    virtual void Idle()=0;
    virtual void Draw(CForm&)=0;
    virtual Clock::time_point Now(){return Clock::now();}
};
class Scope {
public:
    explicit Scope(Backend&);~Scope();
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
private:Backend* backend_{};
};
void Pump();void Idle();void DrawCurrentForm();Clock::time_point Now();
}
