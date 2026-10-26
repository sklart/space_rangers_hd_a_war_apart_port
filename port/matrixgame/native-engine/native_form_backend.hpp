// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "BaseDef.hpp"
class CFormMatrixGame;
namespace srhd_awa::matrixgame::native_form {
class WindowOperations {
public:
    virtual ~WindowOperations()=default;
    virtual void ClientPointToScreen(Base::CPoint&)=0;
    virtual void ClipCursorToWindow()=0;
};
void ClientPointToScreen(Base::CPoint&);
void ClipCursorToWindow();
[[noreturn]] void Screenshot();
// Same HNew(NULL)/FormChange/HDelete ownership as the original launcher.
// Destroy before CGame::Deinit and before detaching platform providers.
class Session {
public:
    Session();~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    CFormMatrixGame& Form()const noexcept{return *form_;}
private:CFormMatrixGame* form_{};
};
}
