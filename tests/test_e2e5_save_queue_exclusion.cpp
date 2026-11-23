#include "e2e5_save_commit_wait.hpp"
#include "e2e4_quicksave_transaction.hpp"

#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace gate = srhd_awa::platform::e2e5_save_commit_wait;
namespace save = srhd_awa::platform::e2e4_quicksave;

int main() {
    // An already existing file cannot distinguish the first failed write from
    // the second successful write when only the shared flag is consulted.
    save::BeginGameSave();
    const bool old_file_exists = true;
    save::BeginGameSave();
    save::CompleteGameSave();
    assert(old_file_exists && save::GameSaveCommitted());

    std::mutex phase_mutex;
    std::condition_variable phase_changed;
    bool first_waiting = false, competitor_tried = false, first_observed = false;
    bool competitor_entered = false;
    std::thread first([&] {
        gate::SaveCall queue; assert(queue.entered());
        { gate::SaveCall nested; assert(!nested.entered()); }
        save::BeginGameSave(); // the worker fails without marking completion
        {
            std::lock_guard<std::mutex> phase(phase_mutex);
            first_waiting = true;
        }
        phase_changed.notify_all();
        {
            std::unique_lock<std::mutex> phase(phase_mutex);
            phase_changed.wait(phase, [&] { return competitor_tried; });
            assert(!competitor_entered);
        }
        assert(old_file_exists && !save::GameSaveCommitted());
        {
            std::lock_guard<std::mutex> phase(phase_mutex);
            first_observed = true;
        }
    });
    std::thread second([&] {
        {
            std::unique_lock<std::mutex> phase(phase_mutex);
            phase_changed.wait(phase, [&] { return first_waiting; });
        }
        // This attempt must not acquire the same queue while the first caller
        // is between completion and inspection of its result.
        const bool acquired = gate::QueueMutex().try_lock();
        if (acquired) gate::QueueMutex().unlock();
        assert(!acquired);
        {
            std::lock_guard<std::mutex> phase(phase_mutex);
            competitor_tried = true;
        }
        phase_changed.notify_all();
        gate::SaveCall queue; assert(queue.entered());
        {
            std::lock_guard<std::mutex> phase(phase_mutex);
            assert(first_observed);
            competitor_entered = true;
        }
        save::BeginGameSave();
        save::CompleteGameSave();
        assert(save::GameSaveCommitted());
    });
    first.join();
    second.join();
    assert(competitor_entered);
    try {
        gate::SaveCall call; assert(call.entered());
        throw std::runtime_error("test unwinding");
    } catch (const std::runtime_error&) {}
    { gate::SaveCall retry; assert(retry.entered()); }
    std::puts("save queue exclusion and result ownership PASS (model)");
}
