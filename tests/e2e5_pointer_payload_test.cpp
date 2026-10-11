#include "e2e5_pointer_payload.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>

using srhd_awa::platform::e2e5_pointer_payload::Registry;

int main() {
  Registry tokens;
  int owner_a = 0;
  int owner_b = 0;
  int script = 42;
  int second_script = 43;
  const auto token = tokens.Register(&owner_a, &script);
  assert(token == tokens.Register(&owner_a, &script));
  assert(tokens.Resolve<int*>(&owner_a, token) == &script);
  assert(tokens.Register(&owner_a, &second_script) != token);
  try {
    (void)tokens.Resolve<int*>(&owner_b, token);
    return 1;
  } catch (const std::runtime_error&) {}
  tokens.Release(&owner_a, token);
  try {
    (void)tokens.Resolve<int*>(&owner_a, token);
    return 2;
  } catch (const std::runtime_error&) {}
  const auto renewed = tokens.Register(&owner_a, &script);
  assert(renewed != token);
  tokens.ReleaseOwner(&owner_a);
  try {
    (void)tokens.Resolve<int*>(&owner_a, renewed);
    return 3;
  } catch (const std::runtime_error&) {}
}
