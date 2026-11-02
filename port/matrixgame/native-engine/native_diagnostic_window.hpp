// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
namespace srhd_awa::matrixgame::diagnostic {
// Own only diagnostic frames. Release before the original Init acquires its
// ordinary platform attachment; reacquire for error UI only after its Deinit.
class WindowPhase {
public:
    explicit WindowPhase(platform3g::Window& window):window_(window){}
    ~WindowPhase(){Release();}
    WindowPhase(const WindowPhase&)=delete;
    void Acquire(){
        if(owner_)throw std::logic_error("diagnostic window phase already owned");
        auto attachment=window_.AttachNative();
        if(!attachment)throw std::logic_error("diagnostic window returned no attachment");
        owner_=std::move(attachment);
    }
    void Release()noexcept{
        if(owner_){owner_->Restore();owner_.reset();}
    }
    bool Owned()const noexcept{return bool(owner_);}
private:
    platform3g::Window& window_;
    std::unique_ptr<platform3g::WindowAttachment> owner_;
};
}
