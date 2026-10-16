#include "e2e59_map_swipe.hpp"
#include "input_platform.hpp"
#include "e2e59_film_index.hpp"
#include <array>
#include <stdexcept>
#include <cassert>
using namespace srhd_awa::platform;
int main() {
  std::array<int, 300> identities{};
  unsigned misses = 0;
  auto cache = MakeFilmIndex<int*>([&](int* pointer) {
    ++misses;
    if (!pointer) return 65535;
    for (unsigned i = 0; i < identities.size(); ++i) if (pointer == &identities[i]) return int(i);
    throw std::runtime_error("foreign film object");
  });
  static_assert(sizeof(cache) < 4096);
  assert(cache.Find(nullptr) == 65535);
  for (unsigned i = 0; i < 100000; ++i) assert(cache.Find(&identities[i % 300]) == int(i % 300));
  int foreign{};
  for (int repeat = 0; repeat < 2; ++repeat) {
    const auto before = misses;
    bool rejected = false;
    try { cache.Find(&foreign); } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected && misses == before + 1);
  }
  MapSwipe swipe;
  const void* screen = &swipe;
  swipe.Begin(screen, 100, 100, 500, 400, 1);
  assert(!swipe.Move(screen, 111, 109));
  assert(swipe.Move(screen, 112, 100));
  assert(swipe.Move(screen, 101, 100)); // Crossing the threshold permanently cancels the tap.
  assert(swipe.center_x + swipe.x - 150 == 450);
  swipe.Cancel();
  assert(!swipe.owner && !swipe.dragging);
  swipe.Begin(screen, 100, 100, 500, 400, 1);
  assert(!swipe.Move(nullptr, 140, 100) && !swipe.dragging);
  input_platform::State state;
  input_platform::RawInput raw{};
  auto input = input_platform::NormalizeAt(&state, raw, 1280, 720, 0);
  raw.touch = true; raw.touch_x = 300; raw.touch_y = 200;
  input = input_platform::NormalizeAt(&state, raw, 1280, 720, 1000);
  assert(input.left_down && input.touch_message && input.touch_held);
  raw.touch = false;
  input = input_platform::NormalizeAt(&state, raw, 1280, 720, 2000);
  assert(input.left_up && input.touch_message && !input.touch_held);
  raw.a = true;
  input = input_platform::NormalizeAt(&state, raw, 1280, 720, 3000);
  assert(input.left_down && !input.touch_message);
}
