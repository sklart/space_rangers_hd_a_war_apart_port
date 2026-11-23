#pragma once

#include <mutex>

// The request belongs to the calling thread. The original save worker never
// reads it; SaveGameToFile uses it before returning to that caller.
namespace srhd_awa::platform::e2e5_save_commit_wait {
inline thread_local bool requested = false;
// One lock covers preparation, publishing and observing the caller's result.
// The worker must never acquire it: callers may wait for it.
inline std::mutex& QueueMutex() { static std::mutex mutex; return mutex; }
inline thread_local bool save_call_active = false;
class SaveCall {
public:
    SaveCall() : lock_(QueueMutex(), std::defer_lock), entered_(!save_call_active) {
        if (entered_) { lock_.lock(); save_call_active = true; }
    }
    ~SaveCall() { if (entered_) save_call_active = false; }
    bool entered() const { return entered_; }
    SaveCall(const SaveCall&) = delete;
    SaveCall& operator=(const SaveCall&) = delete;
private:
    std::unique_lock<std::mutex> lock_;
    bool entered_;
};
class Scope {
public:
    Scope() : previous_(requested) { requested = true; }
    ~Scope() { requested = previous_; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    bool previous_;
};
}
