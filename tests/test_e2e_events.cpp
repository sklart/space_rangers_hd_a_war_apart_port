#include "e2e_events.hpp"

#include <chrono>
#include <cstdint>
#include <thread>

using srhd_awa::platform::e2e_events::Close;
using srhd_awa::platform::e2e_events::Create;
using srhd_awa::platform::e2e_events::Reset;
using srhd_awa::platform::e2e_events::Set;
using srhd_awa::platform::e2e_events::WaitMany;
using srhd_awa::platform::e2e_events::WaitOne;
using srhd_awa::platform::e2e_events::kWaitObject0;
using srhd_awa::platform::e2e_events::kWaitTimeout;

int main() {
  const std::uint32_t automatic = Create(false, false);
  if (!automatic || WaitOne(automatic, 0) != kWaitTimeout) return 1;
  std::uint32_t result = kWaitTimeout;
  std::thread waiter([&] { result = WaitOne(automatic, 500); });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (!Set(automatic)) return 2;
  waiter.join();
  if (result != kWaitObject0 || WaitOne(automatic, 0) != kWaitTimeout) return 3;

  const std::uint32_t manual = Create(true, false);
  if (!Set(manual) || WaitOne(manual, 0) != kWaitObject0 ||
      WaitOne(manual, 0) != kWaitObject0 || !Reset(manual) ||
      WaitOne(manual, 0) != kWaitTimeout) return 4;

  const std::uint32_t handles[2] = {automatic, manual};
  if (!Set(manual) || WaitMany(handles, 2, false, 0) != kWaitObject0 + 1) return 5;
  if (!Set(automatic) || WaitMany(handles, 2, true, 0) != kWaitObject0 ||
      WaitOne(automatic, 0) != kWaitTimeout || WaitOne(manual, 0) != kWaitObject0) return 6;
  if (!Close(automatic) || !Close(manual)) return 7;
}
