int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 GR_Main::CreateStartupLogFile();
 srhd_awa::platform::perf515::Session diagnostics(argv[2]);
 try{
 Empty();Reject([]{m::RangersCampaignSession no_host;});
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);m::RangersScriptSession scripts;Caller caller;
  Reject([]{m::RangersCampaignSession no_ui;});
  m::RangersGlobalUiSession ui;
  const auto previous_screen=GlobalsV::CurrentScreenId;GlobalsV::CurrentScreenId=GlobalsV::screenIntroduction;
  std::puts("CAMPAIGN full_original_UI=PASS original_generation_begin no_save_load=PASS");
  {
   m::RangersCampaignSession campaign;auto& world=campaign.Galaxy();auto* player=aPlayer::GetPlayer();
   assert(aGalaxy::Galaxy==&world && player && player->HomePlanet && player->CurrentStar);
   assert(pas::list_count(world.Stars)==aConst::GalaxyStarCount && pas::list_count(world.Planets)>100 && pas::list_count(world.Rangers)>1);
   assert(pas::list_indexof(world.Stars,player->CurrentStar)>=0 && pas::list_indexof(world.Planets,player->HomePlanet)>=0);
   assert(player->CurrentPlanet || player->DockedTo);assert(player->Name==u"PortDiagnostic");
   assert(pas::list_count(player->StorageEntries)>=2);
   Reject([]{m::RangersCampaignSession duplicate;});assert(aPlayer::GetPlayer()==player);
   std::printf("CAMPAIGN original_generation_complete=PASS stage=%d stars=%d planets=%d rangers=%d turn=%d storage=%d real_Player_Star_HomePlanet=PASS duplicate_owner=REJECTED no_save_load=PASS\n",srhd_awa::platform::e2e3_generation_state::Stage(),pas::list_count(world.Stars),pas::list_count(world.Planets),pas::list_count(world.Rangers),world.CurrentTurn,pas::list_count(player->StorageEntries));
  }
  assert(!aGalaxy::Galaxy && !aGalaxy::PlayerStar && !aPlayer::GetPlayer() && !Globals::NewGameGenerationThread && !aRanger::PlayerOldQuests);
  GlobalsV::CurrentScreenId=previous_screen;
  std::puts("CAMPAIGN original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN");
 }
 Empty();assert(!GR_Main::MainDataConfig && !GR_Main::GlobalCache && !GR_Main::CurrentPixelFormat);
 }catch(const std::exception& e){std::fprintf(stderr,"CAMPAIGN FAILED stage=%d failure_stage=%d error=%s\n",srhd_awa::platform::e2e3_generation_state::Stage(),srhd_awa::platform::e2e3_generation_state::FailureStage(),e.what());return 2;}
}
