#pragma once
#include <cstdint>
struct CtorAllocationResult {std::uint64_t attempts,live,checksum;bool failed;};
CtorAllocationResult ConstructorAllocationProbe(std::int64_t allowed);
