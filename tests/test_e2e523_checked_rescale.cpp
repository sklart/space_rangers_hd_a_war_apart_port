#include "e2e5_checked_rescale.hpp"
#include <cassert>
#include <cstdio>

int main() {
  using srhd_awa::platform::checked_rescale::Invoke;
  errno = ENOMEM;
  bool called = false;
  assert(Invoke([&] { called = true; assert(errno == 0); }));
  assert(called && errno == ENOMEM);
  errno = ENOENT;
  assert(!Invoke([] { errno = ENOMEM; }));
  assert(errno == ENOENT);
  assert(Invoke([] { errno = ERANGE; }));
  assert(errno == ENOENT);
  bool caught = false;
  try {
    Invoke([] { errno = ENOMEM; throw 17; });
  } catch (int value) {
    caught = value == 17;
  }
  assert(caught && errno == ENOENT);
  using srhd_awa::platform::checked_rescale::InvokeWithRecovery;
  int calls = 0, recoveries = 0;
  assert(InvokeWithRecovery([&] { if (++calls == 1) errno = ENOMEM; }, [&] {
    ++recoveries; errno = ERANGE; return true;
  }));
  assert(calls == 2 && recoveries == 1 && errno == ENOENT);
  calls = recoveries = 0;
  assert(!InvokeWithRecovery([&] { ++calls; errno = ENOMEM; }, [&] {
    ++recoveries; return true;
  }));
  assert(calls == 2 && recoveries == 1 && errno == ENOENT);
  calls = recoveries = 0;
  assert(!InvokeWithRecovery([&] { ++calls; errno = ENOMEM; }, [&] {
    ++recoveries; return false;
  }));
  assert(calls == 1 && recoveries == 1 && errno == ENOENT);
  try {
    InvokeWithRecovery([] { errno = ENOMEM; }, []() -> bool {
      errno = ERANGE; throw 19;
    });
    assert(false);
  } catch (int value) { assert(value == 19 && errno == ENOENT); }
  std::puts("checked rescale errno isolation and exception restoration PASS");
}
