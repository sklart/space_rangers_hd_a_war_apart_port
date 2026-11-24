#include "native_vertex_marks.hpp"
#include <cassert>
#include <cstdio>
using srhd_awa::matrixgame::platform3g::NativeVertexMarks;
int main(){
 for(unsigned count:{1u,63u,64u,65u,255u,256u,257u,511u,512u,513u,1024u,8193u}){
  NativeVertexMarks marks(count);
  for(unsigned i=0;i<count;++i)assert(marks.MarkFirst(i));
  for(unsigned i=0;i<count;++i)assert(!marks.MarkFirst(i));
  for(unsigned i=count;i>0;--i)assert(!marks.MarkFirst(i-1));
 }
 NativeVertexMarks empty(0); // No index is valid for an empty table.
 std::puts("vertex mark boundaries, duplicate detection and heap path PASS; CPU ONLY");
}
