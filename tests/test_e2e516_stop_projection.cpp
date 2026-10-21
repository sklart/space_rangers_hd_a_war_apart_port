#include "e2e516_stop_projection.hpp"
#include "units/ab_Global.hpp"
#include <array>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <cstdlib>

int main() {
  using Vector=EC_Struct::TVector3D;
  struct Point { Vector Position; };
  unsigned checks{},hits{};
  for(int rounding:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
    if(std::fesetround(rounding))return 2;
    for(int query=0;query<32;++query) {
      ab_Global::TMatrix4D matrix{};
      // Exercise actual engine projection, including homogeneous division.
      auto* values=reinterpret_cast<double*>(&matrix);
      static_assert(sizeof(matrix)==16*sizeof(double));
      for(int i=0;i<16;++i)values[i]=(i%5==0?1.0:0.0)+(query-16)*(i+1)*0.00013;
      std::array<Point,80> points{};
      for(unsigned i=0;i<points.size();++i)points[i].Position={double(i*13)-400,double(i*7)-200,double(i*3)+1};
      unsigned calls{};
      auto project=[&](const Point* p){++calls;return ab_Global::ProjectPointByMatrix(matrix,pas::ConstRef<Vector>(&p->Position));};
      srhd_awa::platform::StopProjectionMemo<Point,Vector,decltype(project)> memo(project);
      for(unsigned i=0;i<points.size()*4;++i) {
        const auto* p=&points[(i/2)%points.size()]; // Adjacent collision lines share endpoints.
        std::feclearexcept(FE_ALL_EXCEPT);
        const auto expected=ab_Global::ProjectPointByMatrix(matrix,pas::ConstRef<Vector>(&p->Position));
        const int flags=std::fetestexcept(FE_ALL_EXCEPT);
        // Query memoization preserves sticky flags. A fresh exception observation
        // on a cache hit is not the original API's externally observed contract.
        const auto before=calls;const auto actual=memo.Get(p);
        if(std::memcmp(&expected,&actual,sizeof(Vector)) || std::fetestexcept(FE_ALL_EXCEPT)!=flags)std::abort();
        hits+=before==calls;++checks;
      }
    }
  }
  std::fesetround(FE_TONEAREST);
  if(hits==0)return 3;
  std::printf("PASS %u engine projection bit/rounding/sticky-flag comparisons, %u query-local cache hits\n",checks,hits);
}
