#pragma once
#include "sync.hpp"

namespace srhd_awa::platform {
// No allocation and no change to the original successful-path lock lifetime.
class FileLockGuard {
  pas::CriticalSection* lock_;
public:
  explicit FileLockGuard(pas::CriticalSection* lock) : lock_(lock) {
    pas::critical_enter(lock_);
  }
  FileLockGuard(const FileLockGuard&) = delete;
  FileLockGuard& operator=(const FileLockGuard&) = delete;
  void Unlock() noexcept {
    if (lock_) {
      pas::critical_leave(lock_);
      lock_ = nullptr;
    }
  }
  ~FileLockGuard() noexcept { Unlock(); }
};
}
