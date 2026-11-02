// Semantic player commands only. Constructors, prices, resource debits,
// production queues, pathfinding, capture and combat stay in original code.
unsigned campaign_build_commands{};
unsigned campaign_order_polls{};
bool campaign_design_selected{};
int campaign_last_squad_time=-30000;
void CampaignPlayerCommands(){
 auto* side=g_MatrixMap->GetPlayerSide();assert(side && side->m_Constructor && side->m_ConstructPanel && g_IFaceList && g_IFaceList->m_RCountControl);
 std::set<int> groups;int queued=0;
 for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic()){
  if(p->GetSide()!=PLAYER_SIDE)continue;
  if(p->IsLiveRobot() && !p->AsRobot()->GetBase()){
   auto* robot=p->AsRobot();int group=robot->GetGroupLogic();
   // RobotToLogicGroup always allocates a new group and clears its order/path.
   // Existing groups must survive economy polling and resource waits.
   if(group<0 || group>=MAX_LOGIC_GROUP)group=side->RobotToLogicGroup(robot);
   assert(robot->GetGroupLogic()==group);groups.insert(group);
  }
  if(p->IsLiveBuilding() && p->IsBase())queued+=p->AsBuilding()->m_BS.GetItemsCnt();
 }
 // Expand through the original auto-capture command. Polling is independent
 // of the economy so newly built groups also get orders during resource waits.
 if(campaign_order_polls++%4==0){
#if MATRIXGAME_CAMPAIGN_SQUAD
  // Ordinary selection supports at most nine robots. Re-select only to join
  // reinforcements, at most once per 30 seconds, preserving active routes.
  std::vector<CMatrixRobotAI*> selected;
  for(auto* p=CMatrixMapStatic::GetFirstLogic();p && selected.size()<9;p=p->GetNextLogic())
   if(p->GetSide()==PLAYER_SIDE && p->IsLiveRobot() && !p->AsRobot()->GetBase())selected.push_back(p->AsRobot());
  bool joined=true;for(auto* robot:selected)if(robot->GetGroupLogic()!=selected.front()->GetGroupLogic())joined=false;
  if(!selected.empty() && !joined && g_MatrixMap->GetTime()-campaign_last_squad_time>=30000){
   auto* selection=side->GetCurSelGroup();assert(selection);selection->RemoveAll();
   for(auto* robot:selected)selection->AddObject(robot,-4);
   side->CreateGroupFromCurrent();side->Select(selected.size()>1?GROUP:ROBOT,nullptr);
   const int group=side->SelGroupToLogicGroup();assert(group>=0 && group<MAX_LOGIC_GROUP);
   for(auto* robot:selected)assert(robot->GetGroupLogic()==group);
   campaign_last_squad_time=g_MatrixMap->GetTime();
   groups.clear();for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic())if(p->GetSide()==PLAYER_SIDE && p->IsLiveRobot() && !p->AsRobot()->GetBase())groups.insert(p->AsRobot()->GetGroupLogic());
   std::printf("CAMPAIGN_SQUAD original_selection_and_SelGroupToLogicGroup=PASS robots=%zu group=%d map_time=%d\n",selected.size(),group,g_MatrixMap->GetTime());
  }
#endif
  for(int group:groups){assert(group>=0 && group<MAX_LOGIC_GROUP);if(side->m_PlayerGroup[group].Order()!=mpo_AutoCapture)side->PGOrderAutoCapture(group);}
 }
 for(auto* p=CMatrixMapStatic::GetFirstLogic();p;p=p->GetNextLogic()){
  if(p->GetSide()!=PLAYER_SIDE || !p->IsLiveBuilding() || !p->IsBase())continue;
  auto* base=p->AsBuilding();if(base->m_BS.GetItemsCnt()>=6 || side->GetRobotsCnt()+queued>=side->GetMaxSideRobots())continue;
  auto* constructor=side->m_Constructor;constructor->SetBase(base);constructor->OperateCurrentConstruction();
#if MATRIXGAME_CAMPAIGN_HEAVY_DESIGN
  if(!campaign_design_selected){
   // The same public design operation used by CIFaceMenu::CreateItem.
   // No unit is spawned here: RemoteBuild below still charges its real price.
   constructor->SuperDjeans(MRT_CHASSIS,RUK_CHASSIS_TRACK,0);
   constructor->SuperDjeans(MRT_ARMOR,RUK_ARMOR_FULLSTACK,0);
   const auto& layout=g_MatrixMap->m_RobotWeaponMatrix[RUK_ARMOR_FULLSTACK-1];
   assert(layout.common>0 && layout.common<=4);
   for(int slot=0;slot<layout.common;++slot)constructor->SuperDjeans(MRT_WEAPON,RUK_WEAPON_LASER,slot);
   constructor->SuperDjeans(MRT_HEAD,RUK_HEAD_DYNAMO,0);
   campaign_design_selected=true;
   std::printf("CAMPAIGN_PLAYER original_SuperDjeans=PASS track_fullstack_laser_dynamo=PASS common_pilons=%d\n",layout.common);
  }
#endif
  int price[MAX_RESOURCES]{};constructor->GetConstructionPrice(price);int total=0;for(int r=0;r<MAX_RESOURCES;++r){assert(price[r]>=0);total+=price[r];}
  if(!total || !side->IsEnoughResources(price))continue;
  g_IFaceList->m_RCountControl->Reset();assert(g_IFaceList->m_RCountControl->GetCounter()==1);
  int before[MAX_RESOURCES]{};for(int r=0;r<MAX_RESOURCES;++r)before[r]=side->GetResourcesAmount(static_cast<ERes>(r));
  const auto count=base->m_BS.GetItemsCnt();constructor->RemoteBuild(nullptr);
  assert(base->m_BS.GetItemsCnt()==count+1);
  for(int r=0;r<MAX_RESOURCES;++r)assert(side->GetResourcesAmount(static_cast<ERes>(r))==before[r]-price[r]);
  ++queued;++campaign_build_commands;
  std::printf("CAMPAIGN_PLAYER original_RemoteBuild=PASS resource_debit_exact=PASS commands=%u map_time=%d\n",campaign_build_commands,g_MatrixMap->GetTime());
 }
}
