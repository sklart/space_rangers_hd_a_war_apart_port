#pragma once

#include <cerrno>
#include <utility>

namespace srhd_awa::platform::checked_rescale {
namespace detail {
struct RestoreErrno {
  int value{errno};
  ~RestoreErrno() { errno = value; }
};
}

// The pinned OKGF resampler has a void ABI and silently returns when one of
// its required malloc/calloc calls fails. POSIX/newlib report that failure in
// errno. Isolate it from an unrelated caller's errno (notably stale ENOENT),
// and restore the caller's value even when the imported routine throws.
template <class Operation>
bool Invoke(Operation&& operation) {
  detail::RestoreErrno restore;
  errno = 0;
  std::forward<Operation>(operation)();
  return errno != ENOMEM;
}

// Only one recovery attempt. The destination has not been committed by the
// caller, so a retry uses the same still-owned source and destination.
template <class Operation, class Recovery>
bool InvokeWithRecovery(Operation&& operation, Recovery&& recovery) {
  detail::RestoreErrno restore;
  if (Invoke(operation)) return true;
  if (!std::forward<Recovery>(recovery)()) return false;
  return Invoke(operation);
}

}  // namespace srhd_awa::platform::checked_rescale
