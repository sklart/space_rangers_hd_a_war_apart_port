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
  std::puts("checked rescale errno isolation and exception restoration PASS");
}
