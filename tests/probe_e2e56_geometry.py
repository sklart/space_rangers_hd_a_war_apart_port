"""Compare original/optimized geometry, including binary128 on ARM64 QEMU."""
from pathlib import Path
import subprocess
import sys
sys.path.insert(0,str(Path('tools').resolve()))
from e2e56_geometry_overrides import apply
original=Path('upstream/cpp/src/ab_Global.cpp').read_text(encoding='utf-8')
optimized=apply(original)
helpers=Path('upstream/cpp/src/aMyFunction.cpp').read_text(encoding='utf-8')
def extract(text,name):
    start=text.index(name)
    end=text.index('\n    }',start)+len('\n    }')
    return text[start:end]
code='''#include "units/System.hpp"
#include "types/SystemImports.hpp"
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdio>
namespace MathImports {double ArcCos(double v){return std::acos(v);}}
namespace aMyFunction {constexpr long double GamePi=3.1415926L;
'''
for name in ['double HeadingDegreesToRadians(', 'double RadiansToHeadingDegrees(',
             'double HeadingDifferenceDegrees(', 'float WrapHeadingDegrees(']:
    code+=extract(helpers,name)+'\n'
code+='}\n'
for namespace,text in [('reference',original),('optimized',optimized)]:
    code+=f'namespace {namespace} {{\n'
    for name in ['void AdvanceSphericalBearingState(', 'void ComputeSphericalBearingAndDistance(']:
        code+=extract(text,name)+'\n'
    code+='}\n'
code+=r'''
void Equal(double a,double b){assert(std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b));}
int main(){
 std::uint64_t state=314159;
 auto rand=[&](){state^=state<<13;state^=state>>7;state^=state<<17;return state;};
 for(unsigned mode=0;mode<4;++mode){
  pas::set_fpu_control(0x133f | (mode<<10));
  for(unsigned i=0;i<30000;++i){
   const double longitude=(rand()%360000)/1000.;
   const double polar=(rand()%180000)/1000.;
   const double bearing=(rand()%360000)/1000.;
   const double radius=1000.;
   const double distance=(static_cast<int>(rand()%100000)-50000)/100.;
   double a=longitude,b=polar,c=bearing,x=a,y=b,z=c;
   reference::AdvanceSphericalBearingState(pas::Var<double>(&a),pas::Var<double>(&b),pas::Var<double>(&c),radius,distance);
   optimized::AdvanceSphericalBearingState(pas::Var<double>(&x),pas::Var<double>(&y),pas::Var<double>(&z),radius,distance);
   Equal(a,x);Equal(b,y);Equal(c,z);
   double targetlon=(rand()%360000)/1000.,targetpolar=(rand()%180000)/1000.;
   if(i%100==0){targetlon=longitude;targetpolar=polar;}
   reference::ComputeSphericalBearingAndDistance(pas::Var<double>(&a),pas::Var<double>(&b),longitude,polar,bearing,targetlon,targetpolar,radius);
   optimized::ComputeSphericalBearingAndDistance(pas::Var<double>(&x),pas::Var<double>(&y),longitude,polar,bearing,targetlon,targetpolar,radius);
   Equal(a,x);Equal(b,y);
  }
 }
 pas::set_fpu_control(0x133f);
 for(bool fast:{false,true}){
  auto begin=std::chrono::steady_clock::now();volatile double sum=0;
  for(int i=0;i<10000;++i){
   double lon=i%360,polar=70.125,bearing=23.1;
   auto f=fast?optimized::AdvanceSphericalBearingState:reference::AdvanceSphericalBearingState;
   f(pas::Var<double>(&lon),pas::Var<double>(&polar),pas::Var<double>(&bearing),1000,10.123);
   sum=sum+lon+polar+bearing;
  }
  std::printf("geometry %s us=%lld checksum=%.17g\n",fast?"reuse":"reference",
   (long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count(),(double)sum);
 }
 std::puts("120000 state updates + 120000 bearings, 4 rounding modes: bit-identical PASS");
}
'''
out=Path('build/e2e56/geometry-probe');out.mkdir(parents=True,exist_ok=True)
(out/'geometry.cpp').write_text(code,encoding='utf-8')
arm='--arm64' in sys.argv
compiler='aarch64-linux-gnu-g++' if arm else 'g++'
exe=out/('geometry-arm64' if arm else 'geometry-host')
subprocess.run([compiler,'-std=c++20','-O2','-fno-strict-aliasing','-ffunction-sections','-fdata-sections',
 '-Iupstream/cpp/src','-Iupstream/cpp/runtime',str(out/'geometry.cpp'),'upstream/cpp/runtime/runtime.cpp',
 '-Wl,--gc-sections','-o',str(exe),*(['-static'] if arm else [])],check=True)
subprocess.run((['qemu-aarch64'] if arm else [])+[str(exe)],check=True,timeout=120)
