"""Compare effective geometry to the pinned arithmetic on host and ARM64."""
from pathlib import Path
import subprocess
import sys

root = Path.cwd()
sys.path.insert(0, str(root / 'tools'))
from e2e510_overrides import apply
out = root / 'build/e2e510/geometry'
out.mkdir(parents=True, exist_ok=True)
text = (root / 'upstream/cpp/src/aMyFunction.cpp').read_text(encoding='utf-8')
changed = apply('aMyFunction.cpp', text)

def functions(source):
    start = source.index('    float PointDistanceSquared(')
    conditional = '#if defined(__aarch64__)\n'
    if source[:start].endswith(conditional):
        start -= len(conditional)
    end = source.index('\n    //', start)
    integer = source.index('    std::int32_t IntegerPointDistancePlusOne(')
    integer_end = source.index('\n    }', integer) + len('\n    }')
    return source[start:end] + '\n' + source[integer:integer_end]

code = '''#include "units/System.hpp"
#include "types/EC_Struct.hpp"
#include "types/Types.hpp"
#include "e2e5_round.hpp"
#include <bit>
#include <cassert>
#include <cfenv>
#include <chrono>
#include <cstdio>
namespace reference {
''' + functions(text) + '\n}\nnamespace candidate {\n' + functions(changed) + '\n}\n' + r'''
int main() {
 std::uint64_t state = 12345;
 auto next = [&]() { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
 for (unsigned precision : {0x003fu, 0x023fu, 0x033fu}) {
 for (unsigned mode = 0; mode < 4; ++mode) {
  pas::set_fpu_control(precision | (mode << 10));
  for (int x = 0; x <= 46340; ++x) {
   for (int y : {0, 1, 2, 3, 31, 100}) {
    if (std::int64_t(x)*x + y*y > 2147483647) continue;
    Types::TPoint a{x,y}, b{0,0};
    assert(reference::IntegerPointDistancePlusOne(a,b) == candidate::IntegerPointDistancePlusOne(a,b));
   }
  }
  for (unsigned i = 0; i < 250000; ++i) {
   EC_Struct::TPointF a{std::bit_cast<float>(std::uint32_t(next())), std::bit_cast<float>(std::uint32_t(next()))};
   EC_Struct::TPointF b{std::bit_cast<float>(std::uint32_t(next())), std::bit_cast<float>(std::uint32_t(next()))};
   auto x = reference::PointDistance(a,b), y = candidate::PointDistance(a,b);
   assert((std::isnan(x) && std::isnan(y)) || std::bit_cast<std::uint64_t>(x) == std::bit_cast<std::uint64_t>(y));
   auto p = reference::PointDistanceSquared(a,b), q = candidate::PointDistanceSquared(a,b);
   assert((std::isnan(p) && std::isnan(q)) || std::bit_cast<std::uint32_t>(p) == std::bit_cast<std::uint32_t>(q));
   // Stored projection coordinates, including negative ties and range edges.
   double coordinate = (std::int64_t(next() % 10000000) - 5000000) * 0.125;
   assert(System::Round(coordinate) == srhd_awa::platform::e2e5_round::Round(coordinate));
  }
 }
 }
 std::puts("3000000 distance pairs, integer pixel lengths at square boundaries, stored-coordinate round checks, four rounding modes and three precision controls PASS");
 pas::set_fpu_control(0x133f);
 for (bool fast : {false, true}) {
  volatile double checksum = 0;
  auto began = std::chrono::steady_clock::now();
  for (int i = 0; i < 200000; ++i) {
   EC_Struct::TPointF a{float(i % 300), float(i % 89)}, b{float(i % 47), float(i % 29)};
   checksum = checksum + (fast ? candidate::PointDistance(a,b) : reference::PointDistance(a,b));
  }
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count();
  std::printf("geometry fixture cached=%d us=%lld checksum=%.17g\n", fast, (long long)us, double(checksum));
 }
 for (bool fast : {false, true}) {
  volatile std::uint64_t checksum = 0;
  auto began = std::chrono::steady_clock::now();
  for (int i = 0; i < 200000; ++i) {
   Types::TPoint a{i%1280, i%720}, b{i%79, i%57};
   checksum = checksum + (fast ? candidate::IntegerPointDistancePlusOne(a,b) : reference::IntegerPointDistancePlusOne(a,b));
  }
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count();
  std::printf("pixel length fixture fast=%d us=%lld checksum=%llu\n", fast, (long long)us, (unsigned long long)checksum);
 }
}
'''
(out / 'probe.cpp').write_text(code, encoding='utf-8')
common = ['-std=c++20', '-O2', '-frounding-math', '-ffp-contract=off', '-ffunction-sections',
          '-fdata-sections', '-Iport/switch/platform', '-Iupstream/cpp/src', '-Iupstream/cpp/runtime',
          str(out / 'probe.cpp'), 'upstream/cpp/runtime/runtime.cpp', '-Wl,--gc-sections']
for compiler, target, runner in [('g++', 'host', []),
    ('aarch64-linux-gnu-g++', 'arm64', ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu'])]:
    binary = str(out / target)
    subprocess.run([compiler, *common, '-o', binary], check=True)
    subprocess.run([*runner, binary], check=True)
