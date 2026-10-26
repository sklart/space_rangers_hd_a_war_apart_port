// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <stdexcept>
namespace srhd_awa::matrixgame::applet_window {
enum class Focus {Foreground,Unfocused,Background};
struct Event {Focus focus;bool exit{};};
struct ExitRequested:std::runtime_error {ExitRequested():runtime_error("native applet exit requested") {}};
class Backend {
public:
    virtual ~Backend()=default;
    virtual void Start()=0;
    virtual void Stop()noexcept=0;
    virtual Event Read()=0;
    // Consume at most one queued notification or IPC message per call.
    virtual bool Next(Event&)=0;
    virtual void Wait()=0;
    virtual void SetVisible(bool)=0;
    virtual std::uint32_t RestoreVisible(bool)noexcept=0;
};
class Observer {
public:
    virtual ~Observer()=default;
    virtual void FocusChanged(bool)=0;
    virtual void Exit()noexcept=0;
};
// Visibility has no libnx getter: the borrowing owner must supply its baseline.
// Neither focus/suspend policy nor ownership of NWindow/applet services changes.
class Lifecycle {
public:
    Lifecycle(Backend& backend,Observer& observer,bool baseline):backend_(backend),observer_(observer),baseline_(baseline),visible_(baseline){}
    ~Lifecycle(){Detach();}
    Lifecycle(const Lifecycle&)=delete;
    void Attach();
    std::uint32_t Detach()noexcept;
    void Show(bool);
    void Update();
    void WaitForDrawable();
    bool Attached()const noexcept{return attached_;}
    bool Drawable()const noexcept{return attached_ && visible_ && foreground_ && !exit_;}
    std::uint32_t RestoreResult()const noexcept{return restore_result_;}
private:
    void Accept(Event);
    Backend& backend_;Observer& observer_;
    bool baseline_,visible_,attached_{},known_{},foreground_{},exit_{};
    std::uint32_t restore_result_{};
};
}
