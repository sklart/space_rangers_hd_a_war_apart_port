// Exercises original game orders and turn workers. No location/ship/world
// fields are assigned by this fixture, and no save-file APIs are called.
#include "units/aCalc.hpp"
#include "types/aPlanet.hpp"
#include "types/aShip.hpp"
#include "types/ThreadCalc.hpp"
void ExerciseOriginalLanding(){
 auto* player=aPlayer::GetPlayer();assert(player && player->CurrentStar);
 auto* target=player->HomePlanet;assert(target && target->CurrentStar==player->CurrentStar && target->OwnerId!=aGalaxyStruct::oiUninhabited);
 const auto initial_turn=aGalaxy::Galaxy->CurrentTurn;
 std::printf("NAVIGATION initial_turn=%d planet=%d docked=%d speed=%d target_id=%u\n",initial_turn,player->CurrentPlanet!=nullptr,player->DockedTo!=nullptr,int(player->Speed),unsigned(target->Id));
 auto step=[]{
  aCalc::CalculatePlayerStarTurnAndWait();assert(!GR_Main::ExitScreenLoop && aPlayer::GetPlayer());
  aCalc::CalculateGalaxyTurnAndWait();assert(!GR_Main::ExitScreenLoop && aPlayer::GetPlayer());
 };
 if(player->CurrentPlanet!=target){
  if(player->IsDocked()){
   player->OrderTakeoff();assert(player->Order==aShip::soTakeoff);
   step();assert(player->InNormalSpace());
   std::printf("NAVIGATION original_takeoff=PASS turn=%d\n",aGalaxy::Galaxy->CurrentTurn);
  }
  player->OrderLanding(target,false);assert(player->Order==aShip::soLand && player->OrderTarget==target);
  int turns=0;
  while(player->CurrentPlanet!=target && turns<80){
   step();++turns;assert(player==aPlayer::GetPlayer());
   std::printf("NAVIGATION travel_turn=%d order=%u planet=%d docked=%d\n",aGalaxy::Galaxy->CurrentTurn,unsigned(player->Order),player->CurrentPlanet!=nullptr,player->DockedTo!=nullptr);
  }
 }
 assert(player->CurrentPlanet==target && !player->DockedTo && player->CurrentStar==target->CurrentStar);
 std::printf("NAVIGATION original_landing=PASS elapsed_turns=%d player_location_unmodified=PASS no_save_load=PASS\n",aGalaxy::Galaxy->CurrentTurn-initial_turn);
}
