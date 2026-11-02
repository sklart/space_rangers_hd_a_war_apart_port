// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_device_state.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
namespace p=srhd_awa::matrixgame::platform3g;
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
 p::NativeStateStore store;
 if(argc==2 && !std::strcmp(argv[1],"--baseline")){
  Reject([&]{store.Edit().Render(161,1);});
  std::puts("BASELINE original UI restore MULTISAMPLEANTIALIAS=TRUE rejected on single-sample native state: REPRODUCED");return 0;
 }
 for(unsigned enabled:{0u,1u})for(unsigned mask:{0u,1u,0x55555555u,~0u}){
  store.Edit().Render(161,enabled);store.Edit().Render(162,mask);
  assert(store.Read().Render(161)==enabled && store.Read().Render(162)==mask);
  store.Read().ValidateDraw();store.Read().ValidateClear();
  {auto saved=store.Capture();store.Edit().Render(161,1-enabled);store.Edit().Render(162,~mask);
   assert(saved->Restore()==0);}
  assert(!store.Outstanding() && store.Read().Render(161)==enabled && store.Read().Render(162)==mask);
 }
 store.Edit().Render(161,1);Reject([&]{store.Edit().Render(161,2);});assert(store.Read().Render(161)==1);
#ifdef MATRIXGAME_SINGLE_SAMPLE_CANDIDATE
 for(unsigned enabled:{0u,1u}){store.Edit().Render(161,enabled);
  Reject([&]{store.Read().ValidateDraw(false,false,2);});
  Reject([&]{store.Read().ValidateDraw(false,false,0,1);});}
#endif
 std::puts("single-sample render state TRUE/FALSE masks=4 exact capture/restore invalid BOOL rollback=PASS CPU_STATE_ONLY NO_GPU");
}
