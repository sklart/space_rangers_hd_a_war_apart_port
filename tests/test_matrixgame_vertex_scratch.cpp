#include "native_vertex_scratch.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
using srhd_awa::matrixgame::platform3g::NativeVertexScratch;
struct Vertex {int x{17};int y{42};};
struct MayThrow {static bool fail;MayThrow(){if(fail)throw std::runtime_error("inject");}};bool MayThrow::fail=false;
int main(){
 NativeVertexScratch<Vertex> pool;Vertex* prior;
 {auto a=pool.Acquire(8);prior=a.Values().data();a.Values()[0].x=99;
  auto b=pool.Acquire(8);assert(b.Values().data()!=prior);b.Values()[0].x=13;
  auto c=pool.Acquire(8);assert(c.Values().data()!=prior && c.Values().data()!=b.Values().data());
  assert(a.Values()[0].x==99 && b.Values()[0].x==13);
 }
 {auto a=pool.Acquire(8);assert(a.Values().data()==prior && a.Values()[0].x==17 && a.Values()[0].y==42);}
 {auto a=pool.Acquire(5000);assert(a.Values().size()==5000);}
 {auto a=pool.Acquire(1);assert(a.Values().capacity()<=4096 && a.Values()[0].x==17);}
 NativeVertexScratch<MayThrow> failing;MayThrow::fail=true;
 try{auto x=failing.Acquire(1);assert(false);}catch(const std::runtime_error&){}
 MayThrow::fail=false;{auto x=failing.Acquire(1);assert(x.Values().size()==1);}
 std::puts("scratch reuse, reset, nesting, oversized release and failure recovery PASS; CPU ONLY");
}
