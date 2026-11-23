#pragma once
#include <atomic>
namespace matrixgame_worker_failure_probe {
inline std::atomic<bool> fail_before_replace{false};
}
