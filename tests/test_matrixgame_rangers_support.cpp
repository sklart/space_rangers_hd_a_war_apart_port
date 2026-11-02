#ifdef NDEBUG
#error Rangers support policy requires assertions
#endif
#include "native_rangers_support.hpp"
#include <cassert>
#include <cstdio>
using srhd_awa::matrixgame::ClassifyRangersSupport;
int main(){
 assert(ClassifyRangersSupport({false,8,16384,16})==1);
 assert(ClassifyRangersSupport({true,1,16384,16})==3);
 assert(ClassifyRangersSupport({true,2,2047,16})==3);
 assert(ClassifyRangersSupport({true,2,2048,0})==2);
 assert(ClassifyRangersSupport({true,2,2048,1})==0);
 assert(ClassifyRangersSupport({true,8,8192,1})==0);
 assert(ClassifyRangersSupport({true,8,16384,16})==0);
 assert(ClassifyRangersSupport({true,0,0,0})==3);
 std::puts("RANGERS_SUPPORT original_code_order=PASS texture_width_boundary=PASS stream_boundary=PASS query_failure=PASS CPU_POLICY_ONLY");
}
