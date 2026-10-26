// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_loop.hpp"
#include "Form.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame::native_loop {
namespace {
thread_local Backend* current{};
Backend& Current(){if(!current)throw std::logic_error("native original loop has no backend");return *current;}
}
Scope::Scope(Backend& backend):backend_(&backend){if(current)throw std::logic_error("recursive native loop backend");current=&backend;}
Scope::~Scope(){if(current!=backend_)std::terminate();current=nullptr;}
void Pump(){Current().Pump();}void Idle(){Current().Idle();}
void DrawCurrentForm(){if(!g_FormCur)throw std::logic_error("native original loop lost active form");Current().Draw(*g_FormCur);}
Clock::time_point Now(){return Current().Now();}
}
