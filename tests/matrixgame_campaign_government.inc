// Open the real government form only after real landing. No synthetic Player,
// Planet, battle result, or government dialogue is provided.
void ExerciseOriginalGovernment(){
 auto* player=aPlayer::GetPlayer();assert(player && player->CurrentPlanet && !player->DockedTo);
 auto* gov=Globals::GovernmentScreen;assert(gov && gov->PendingTransition==0);
 // InitializeGlobalUiRuntime loads the config tree. The separate original
 // layout phase binds MainPanel/PlanetPanel/LoadPanel to their owning screen.
 gov->InitializeLayout();assert(gov->MainPanel->Screen==gov);
 const auto previous=GlobalsV::CurrentScreenId;GlobalsV::CurrentScreenId=GlobalsV::screenGovernment;
 std::puts("GOVERNMENT original_OnOpen_begin real_campaign_planet=PASS");
 gov->OnOpen();assert(!GR_Main::ExitScreenLoop && aPlayer::GetPlayer()==player && gov->PendingTransition==0);
 assert(gov->PortraitPanel && gov->PortraitPanel->Active && gov->DialogText.length()>0);
 std::printf("GOVERNMENT original_OnOpen=PASS dialog_units=%zu real_campaign_planet=PASS battle_result=NOT_RUN\n",std::size_t(gov->DialogText.length()));
 gov->OnClose();GlobalsV::CurrentScreenId=previous;
 std::puts("GOVERNMENT original_OnClose=PASS");
}
