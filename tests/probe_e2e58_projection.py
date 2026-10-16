"""Compare cached projection to the original Extended projector on host and ARM64."""
from pathlib import Path
import subprocess

root = Path.cwd()
out = root / 'build/e2e58/projection'
out.mkdir(parents=True, exist_ok=True)
text = (root / 'upstream/cpp/src/ab_Global.cpp').read_text(encoding='utf-8')
start = text.index('    EC_Struct::TVector3D ProjectPointByMatrix(')
end = text.index('\n    }', start) + len('\n    }')
code = '''#include "types/ab_Global.hpp"
#include "types/EC_Struct.hpp"
#include "e2e58_projection_cache.hpp"
#include <cassert>
#include <chrono>
#include <cstdio>
namespace original {
using ab_Global::TMatrix4D;
''' + text[start:end] + '\n}\n' + r'''
int main(){
 std::uint64_t state=314159;
 auto rand=[&](){state^=state<<13;state^=state>>7;state^=state<<17;return state;};
 unsigned calls=0;
 for(unsigned mode=0;mode<4;++mode){
  pas::set_fpu_control(0x133f|(mode<<10));
  for(unsigned frame=0;frame<200;++frame){
   ab_Global::TMatrix4D matrix{};
   for(auto& row:matrix.elements) for(auto& x:row.elements) x=(int(rand()%2000)-1000)/127.;
   auto cache=srhd_awa::platform::MakeProjectionCache<EC_Struct::TVector3D>(
    [&](const auto& point){++calls;return original::ProjectPointByMatrix(matrix,pas::ConstRef<EC_Struct::TVector3D>(&point));});
   std::array<EC_Struct::TVector3D,300> points{};
   for(auto& p:points) p={(int(rand()%20000)-10000)/13.,(int(rand()%20000)-10000)/19.,(int(rand()%20000)-10000)/23.};
   points[0]={0.,-0.,0.};points[1]={-0.,0.,-0.};
   for(unsigned i=0;i<6000;++i){
    const auto& p=points[(i/2)%points.size()];
    auto a=original::ProjectPointByMatrix(matrix,pas::ConstRef<EC_Struct::TVector3D>(&p));
    auto b=cache.Project(p);
    assert(std::bit_cast<std::uint64_t>(a.X)==std::bit_cast<std::uint64_t>(b.X));
    assert(std::bit_cast<std::uint64_t>(a.Y)==std::bit_cast<std::uint64_t>(b.Y));
    assert(std::bit_cast<std::uint64_t>(a.Z)==std::bit_cast<std::uint64_t>(b.Z));
   }
  }
 }
 assert(calls<2400000);
 std::printf("original Extended projection: 4800000 bitwise comparisons, calls=%u, 4 rounding modes PASS\n",calls);
 pas::set_fpu_control(0x133f);
 ab_Global::TMatrix4D matrix{};
 for(int i=0;i<4;++i) for(int j=0;j<4;++j) matrix[i][j]=(i*4+j+1)/127.;
 std::array<EC_Struct::TVector3D,300> points{};
 for(unsigned i=0;i<points.size();++i) points[i]={i/13.,i/19.,i/23.};
 double reference_sum=0;
 for(bool fast:{false,true}){
  auto cache=srhd_awa::platform::MakeProjectionCache<EC_Struct::TVector3D>(
    [&](const auto& p){return original::ProjectPointByMatrix(matrix,pas::ConstRef<EC_Struct::TVector3D>(&p));});
  volatile double sum=0;
  auto began=std::chrono::steady_clock::now();
  for(unsigned i=0;i<100000;++i){
   const auto& p=points[(i/2)%points.size()];
   auto value=fast?cache.Project(p):original::ProjectPointByMatrix(matrix,pas::ConstRef<EC_Struct::TVector3D>(&p));
   sum=sum+value.X+value.Y+value.Z;
  }
  auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-began).count();
  if(!fast)reference_sum=sum;else assert(std::bit_cast<std::uint64_t>(double(sum))==std::bit_cast<std::uint64_t>(reference_sum));
  std::printf("projection microbenchmark cached=%d us=%lld checksum=%.17g\n",fast,(long long)us,double(sum));
 }
}
'''
(out / 'probe.cpp').write_text(code)
includes = ['-Iport/switch/platform', '-Iupstream/cpp/src', '-Iupstream/cpp/runtime']
common = ['-std=c++20', '-O2', '-ffunction-sections', '-fdata-sections',
          *includes, str(out/'probe.cpp'), 'upstream/cpp/runtime/runtime.cpp', '-Wl,--gc-sections']
subprocess.run(['g++', *common, '-o', str(out/'host')], check=True)
subprocess.run([str(out/'host')], check=True)
subprocess.run(['aarch64-linux-gnu-g++', *common, '-o', str(out/'arm64')], check=True)
subprocess.run(['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu', str(out/'arm64')], check=True)
