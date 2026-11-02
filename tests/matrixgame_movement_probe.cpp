#ifdef NDEBUG
#error Movement verification requires assertions
#endif
#include "matrixgame_movement_probe.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixRobot.hpp"
#include "MatrixSide.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
MovementProbe::~MovementProbe(){if(core_)core_->Release();}
void MovementProbe::Issue(){
 assert(!core_ && g_MatrixMap && !g_MatrixMap->IsPaused());CMatrixRobotAI* robot{};
 for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic())if(p->IsLiveRobot() && p->GetSide()==PLAYER_SIDE && !p->AsRobot()->GetBase()){robot=p->AsRobot();break;}
 assert(robot && robot->m_UnitCnt);x_=robot->GetMapPosX();y_=robot->GetMapPosY();world_x_=robot->m_PosX;world_y_=robot->m_PosY;
 target_x_=x_+10;target_y_=y_;
 const auto chassis=robot->m_Unit[0].u1.s1.m_Kind;
 assert(g_MatrixMap->PlaceFindNear(int(chassis)-1,ROBOT_MOVECELLS_PER_SIZE,target_x_,target_y_,robot));
 const int dx=target_x_-x_,dy=target_y_-y_;assert(dx*dx+dy*dy>=36);
 core_=robot->GetCore(DEBUG_CALL_INFO);time_=g_MatrixMap->GetTime();
 auto* side=g_MatrixMap->GetPlayerSide();const auto group=side->RobotToLogicGroup(robot);assert(group>=0 && group<MAX_LOGIC_GROUP);
 side->PGOrderMoveTo(group,CPoint(target_x_,target_y_));
 std::printf("MOVEMENT_COMMAND player_group=%d start_cell=%d,%d target_cell=%d,%d world=%.6f,%.6f map_time=%d original_PlaceFindNear_PGOrderMoveTo=EXECUTED\n",group,x_,y_,target_x_,target_y_,world_x_,world_y_,time_);
}
void MovementProbe::Verify(){
 assert(core_ && core_->m_Object && core_->m_Object->IsLiveRobot());auto* robot=core_->m_Object->AsRobot();
 const auto elapsed=g_MatrixMap->GetTime()-time_;const float dx=robot->m_PosX-world_x_,dy=robot->m_PosY-world_y_;
 const int remaining_x=target_x_-robot->GetMapPosX(),remaining_y=target_y_-robot->GetMapPosY();
 const int initial_x=target_x_-x_,initial_y=target_y_-y_;
 std::printf("MOVEMENT_RESULT elapsed=%d start_cell=%d,%d end_cell=%d,%d target_cell=%d,%d delta_world=%.6f,%.6f remaining2=%d initial2=%d orders=%d\n",elapsed,x_,y_,robot->GetMapPosX(),robot->GetMapPosY(),target_x_,target_y_,dx,dy,remaining_x*remaining_x+remaining_y*remaining_y,initial_x*initial_x+initial_y*initial_y,robot->GetOrdersInPool());
 assert(elapsed==5000 && std::isfinite(dx) && std::isfinite(dy));
 assert(dx*dx+dy*dy>=GLOBAL_SCALE_MOVE*GLOBAL_SCALE_MOVE);
 assert(remaining_x*remaining_x+remaining_y*remaining_y<initial_x*initial_x+initial_y*initial_y);
 assert(robot->GetMapPosX()!=x_ || robot->GetMapPosY()!=y_);
 std::puts("MOVEMENT original_player_command_navigation_simulation=PASS no_position_injection=PASS live_robot=PASS");
}
