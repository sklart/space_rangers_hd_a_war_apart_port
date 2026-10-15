#include "e2e3_generation_state.hpp"

#include <cassert>
#include <thread>

namespace state = srhd_awa::platform::e2e3_generation_state;

int main() {
  state::Reset();
  assert(state::Stage() == 0 && !state::Failed());
  std::thread worker([] {
    state::SetStage(1);
    state::SetStage(6);
    state::Fail(6);
  });
  worker.join();
  assert(state::Stage() == 6 && state::Failed() && state::FailureStage() == 6);
  assert(state::Stage() != 8);  // An error cannot masquerade as completion.
  state::Reset();
  assert(state::Stage() == 0 && !state::Failed());
  std::thread success([] { state::SetStage(8); });
  success.join();
  assert(state::Stage() == 8 && !state::Failed());
}
