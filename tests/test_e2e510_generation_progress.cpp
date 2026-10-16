#include "e2e3_generation_state.hpp"
#include <cassert>
#include <thread>
#include <cstdio>
int main() {
  using namespace srhd_awa::platform::e2e3_generation_state;
  Reset();
  assert(Progress(1) == 0);
  Attempt(1, 7);
  assert(Progress(1) == -7);
  Work(1, 58, 100);
  assert(Progress(1) == 59);
  Work(1, -100, 100);
  assert(Progress(1) == 1);
  Work(1, 999, 100);
  assert(Progress(1) == 101);
  SetStage(1);
  assert(Progress(0) == 101);
  SetStage(2);
  assert(Progress(1) == 101);
  Work(2, 2147483646, 2147483647);
  assert(Progress(2) == 100);
  Reset();
  std::atomic<bool> done{false};
  std::thread worker([&] {
    for (int i = 0; i <= 100000; ++i) Work(6, i, 100000);
    done.store(true);
  });
  int previous = 0;
  while (!done.load()) {
    const int value = Progress(6);
    assert(value >= previous && value >= 0 && value <= 101);
    previous = value;
  }
  worker.join();
  assert(Progress(6) == 101);
  Reset();
  assert(Progress(6) == 0);
  std::puts("generation progress: counts, attempts, clamping, reset and worker/UI publication PASS");
}
