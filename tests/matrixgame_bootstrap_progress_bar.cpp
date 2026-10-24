#ifdef NDEBUG
#error Progress-bar lifecycle verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixProgressBar.hpp"
#include "StringConstants.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapProgressBar(NativeInitRuntime& runtime) {
 assert(runtime.CpuReady() && !runtime.GpuReady() && g_MatrixHeap && g_Cache);
 auto* texture=legacy_texture::GetManaged(TEXTURE_PATH_PB);assert(texture->Ref()==0 && !texture->IsLoaded());
 for(int cycle=0;cycle<2;++cycle) {
  auto* first=HNew(g_MatrixHeap) CMatrixProgressBar();
  auto* second=HNew(g_MatrixHeap) CMatrixProgressBar();assert(texture->Ref()==2);
  first->Modify(12.25f,-7.75f,80.9f,0.4f);second->Modify(10,20,50,0.8f);
  assert(first->ClonePresent(PBC_ORIGINAL) && !first->ClonePresent(PBC_CLONE1));
  first->CreateClone(PBC_CLONE1,1,2,30);first->CreateClone(PBC_CLONE2,4,5,40);
  assert(first->ClonePresent(PBC_CLONE1) && first->ClonePresent(PBC_CLONE2));
  first->KillClone(PBC_CLONE1);assert(!first->ClonePresent(PBC_CLONE1) && first->ClonePresent(PBC_CLONE2));
  first->KillClone(PBC_CLONE2);assert(!first->ClonePresent(PBC_CLONE2));
  second->CreateClone(PBC_CLONE2,5,6,70);assert(second->ClonePresent(PBC_CLONE2));
  if(!cycle){HDelete(CMatrixProgressBar,first,g_MatrixHeap);assert(texture->Ref()==1);HDelete(CMatrixProgressBar,second,g_MatrixHeap);}
  else{HDelete(CMatrixProgressBar,second,g_MatrixHeap);assert(texture->Ref()==1);HDelete(CMatrixProgressBar,first,g_MatrixHeap);}
  assert(texture->Ref()==0 && !texture->IsLoaded() && legacy_texture::GetManaged(TEXTURE_PATH_PB)==texture);
 }
 std::puts("original progress bar ctor/shared texture refs/clones/destructor/reentry EXECUTED; parent cache retains actual unloaded texture; draw NOT EXECUTED");
}
