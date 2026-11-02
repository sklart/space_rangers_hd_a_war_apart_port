// Original methods on an idle genuine world robot; no simulated device success.
#ifdef NDEBUG
#error Robot order verification requires assertions
#endif
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixRobot.hpp"
#include <sanitizer/asan_interface.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace {
void Empty(CMatrixRobotAI& robot){while(robot.GetOrdersInPool())robot.RemoveOrder(0);}
void Fill(CMatrixRobotAI& robot,int count){
 Empty(robot);for(int i=0;i<count;++i){auto* order=robot.AllocPlaceForOrderOnTop();assert(order);order->SetOrder(ROT_MOVE_TO,0,0,0);}
 for(int i=0;i<count;++i){auto* order=robot.GetOrder(i);order->SetOrder(ROT_MOVE_TO,float(i)+.25f,float(i)+.5f,float(i)+.75f,100+i);order->SetPhase(ROP_WAITING_FOR_PARAMS);}
}
void Equal(const SOrder& a,const SOrder& b){assert(a.m_OrderType==b.m_OrderType && a.m_OrderPhase==b.m_OrderPhase && a.m_Param1==b.m_Param1 && a.m_Param2==b.m_Param2 && a.m_Param3==b.m_Param3 && a.m_Param4==b.m_Param4 && a.m_ObjCore==b.m_ObjCore);}
void Rotate(CMatrixRobotAI& robot){
 const auto count=robot.GetOrdersInPool();
 // Logical array boundary: inactive capacity/next member is not an input to
 // rotation. Standard ASan would not otherwise flag an intra-object overread.
 auto* boundary=robot.GetOrder(count);__asan_poison_memory_region(boundary,sizeof(SOrder));
 robot.ProcessOrdersList();__asan_unpoison_memory_region(boundary,sizeof(SOrder));
}
}
void VerifyRobotOrders(){
 CMatrixRobotAI* robot{};
 for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic())if(p->IsRobot()){
  auto* candidate=p->AsRobot();if(!candidate->GetOrdersInPool() && !candidate->GetBase()){robot=candidate;break;}
 }
 assert(robot);unsigned removals{},rotations{};
#ifndef ORDER_TEST_ROTATE_ONLY
 for(int count=1;count<=MAX_ORDERS;++count)for(int pos=0;pos<count;++pos){
  Fill(*robot,count);std::array<SOrder,MAX_ORDERS> before{};for(int i=0;i<count;++i)before[i]=*robot->GetOrder(i);
  robot->RemoveOrder(-1);robot->RemoveOrder(count);assert(robot->GetOrdersInPool()==count);
  robot->RemoveOrder(pos);assert(robot->GetOrdersInPool()==count-1);
  for(int i=0;i<count-1;++i)Equal(*robot->GetOrder(i),before[i+(i>=pos)]);++removals;
 }
#endif
 Empty(*robot);
 for(int count=0;count<=MAX_ORDERS;++count){
  Fill(*robot,count);std::array<SOrder,MAX_ORDERS> before{};for(int i=0;i<count;++i)before[i]=*robot->GetOrder(i);
  for(int step=1;step<=2*MAX_ORDERS;++step){Rotate(*robot);assert(robot->GetOrdersInPool()==count);for(int i=0;i<count;++i)Equal(*robot->GetOrder(i),before[(i+step)%count]);++rotations;}
 }
 Empty(*robot);
 // Genuine SObjectCore ownership survives rotation and is released exactly once.
 auto* core=robot->GetCore(DEBUG_CALL_INFO);const int held=core->m_Ref;
 for(int i=0;i<3;++i){auto* order=robot->AllocPlaceForOrderOnTop();assert(order);order->SetOrder(ROT_FIRE,robot);}
 assert(core->m_Ref==held+3);Rotate(*robot);assert(core->m_Ref==held+3);
 Empty(*robot);assert(core->m_Ref==held);core->Release();
 std::printf("ROBOT_ORDER genuine_idle_world_robot=PASS removals=%u rotations=%u all_fields=PASS logical_read_boundary=PASS object_core_refs=PASS original_empty_restored=PASS\n",removals,rotations);
}
