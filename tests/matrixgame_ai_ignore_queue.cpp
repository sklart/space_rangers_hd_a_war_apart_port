// Real CInfo against genuine loaded map identities/time. No device substitutes.
#ifdef NDEBUG
#error AI ignore regression requires assertions
#endif
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixMapStatic.hpp"
#include "Logic/MatrixEnvironment.h"
#include <array>
#include <cassert>
#include <cstdio>
void VerifyAiIgnoreQueue(){
 assert(g_MatrixMap);std::array<CMatrixMapStatic*,17> objects{};unsigned count{};
 for(auto* p=CMatrixMapStatic::GetFirstLogic();p && count<objects.size();p=p->GetNextLogic())objects[count++]=p;
 assert(count==objects.size());const auto now=g_MatrixMap->GetTime();
 CInfo info;
 for(unsigned i=0;i<16;++i){info.AddIgnore(objects[i]);info.m_IgnoreTime[i]=now-100+int(i);}
 info.AddIgnore(objects[16]);assert(info.m_IgnoreCnt==16);
 for(unsigned i=0;i<15;++i){assert(info.m_Ignore[i]==objects[i+1]);assert(info.m_IgnoreTime[i]==now-99+int(i));}
 assert(info.m_Ignore[15]==objects[16] && info.m_IgnoreTime[15]==now);
 assert(!info.IsIgnore(objects[0]) && info.IsIgnore(objects[16]));
 for(unsigned insertion=17;insertion<81;++insertion){
  auto* next=objects[insertion%objects.size()];const auto before=info.m_Ignore;
  // Last evicted identity is the next unique insertion in this 17-item ring.
  std::array<CMatrixMapStatic*,15> expected{};for(unsigned i=0;i<15;++i)expected[i]=before[i+1];
  info.AddIgnore(next);assert(info.m_IgnoreCnt==16);
  for(unsigned i=0;i<15;++i)assert(info.m_Ignore[i]==expected[i]);
  assert(info.m_Ignore[15]==next && info.m_IgnoreTime[15]==now && info.IsIgnore(next));
 }
 auto* duplicate=info.m_Ignore[4];info.m_IgnoreTime[4]=now-50;info.AddIgnore(duplicate);assert(info.m_IgnoreTime[4]==now && info.m_IgnoreCnt==16);
 auto* absent=objects[81%objects.size()];assert(!info.IsIgnore(absent));
 auto* removed=info.m_Ignore[3];info.DelIgnore(removed);assert(info.m_Ignore[3]==nullptr);info.AddIgnore(absent);assert(info.m_Ignore[3]==absent && info.m_IgnoreTime[3]==now);
 info.m_IgnoreTime[2]=now-5000;auto* expired=info.m_Ignore[2];assert(!info.IsIgnore(expired));info.AddIgnore(expired);assert(info.IsIgnore(expired) && info.m_IgnoreTime[2]==now);
 info.m_IgnoreTime[2]=now-5000;info.AddIgnore(removed);assert(info.m_Ignore[2]==removed && info.m_IgnoreTime[2]==now && info.IsIgnore(removed));
 std::puts("AI_IGNORE real_map_identities=17 overflow_shifts=65 FIFO_pointer_time=PASS fresh_timestamp=PASS duplicate_refresh=PASS deleted_slot=PASS expiry_boundary=PASS");
}
