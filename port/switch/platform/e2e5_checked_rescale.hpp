#pragma once

#include <cerrno>
#include <utility>

namespace srhd_awa::platform::checked_rescale {

// The pinned OKGF resampler has a void ABI and silently returns when one of
// its required malloc/calloc calls fails. POSIX/newlib report that failure in
// errno. Isolate it from an unrelated caller's errno (notably stale ENOENT),
// and restore the caller's value even when the imported routine throws.
template <class Operation>
bool Invoke(Operation&& operation) {
  const int previous = errno;
  struct Restore {
    int value;
    ~Restore() { errno = value; }
  } restore{previous};
  errno = 0;
  std::forward<Operation>(operation)();
  return errno != ENOMEM;
}

}  // namespace srhd_awa::platform::checked_rescale
