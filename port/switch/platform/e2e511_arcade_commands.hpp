#pragma once

namespace srhd_awa::platform {
// Search at most one inventory lap; empty groups leave the selection alone.
template<class IsSecondary>
int NextArcadeWeapon(int current, int count, bool secondary, IsSecondary group) {
  if (count < 1 || count > 5) return -1;
  if (current < 0 || current >= count) current = count - 1;
  for (int step = 1; step <= count; ++step) {
    const int candidate = (current + step) % count;
    if (group(candidate) == secondary) return candidate;
  }
  return -1;
}
}
