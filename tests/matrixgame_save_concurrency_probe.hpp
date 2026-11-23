// Test-only synchronization for actual SaveGameToFile calls in one campaign.
#pragma once
#include "e2e5_save_commit_wait.hpp"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace matrixgame_save_concurrency_probe {
inline thread_local int role = 0;
inline std::mutex mutex;
inline std::condition_variable changed;
inline bool first_waiting = false;
inline bool second_attempted = false;
inline void BeforeEntry() {
    if (role != 2) return;
    std::unique_lock<std::mutex> lock(mutex);
    assert(changed.wait_for(lock, std::chrono::seconds(30), [] { return first_waiting; }));
    const bool acquired = srhd_awa::platform::e2e5_save_commit_wait::QueueMutex().try_lock();
    if (acquired) srhd_awa::platform::e2e5_save_commit_wait::QueueMutex().unlock();
    assert(!acquired);
    second_attempted = true;
    lock.unlock(); changed.notify_all();
}
inline void AfterFirstWait() {
    if (role != 1) return;
    std::unique_lock<std::mutex> lock(mutex);
    first_waiting = true;
    changed.notify_all();
    assert(changed.wait_for(lock, std::chrono::seconds(30), [] { return second_attempted; }));
}
}
