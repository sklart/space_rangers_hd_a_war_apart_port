// Reproduce the real Gov pre-save context while keeping a disposable save path.
Globals::GalaxyScreen->InitializeLayout();
auto* gov = Globals::GovernmentScreen;
gov->InitializeLayout();
const auto previous_screen_id = GlobalsV::CurrentScreenId;
auto* previous_stack = GI_MessageLoop::MessageLoopStack;
GlobalsV::CurrentScreenId = GlobalsV::screenGovernment;
GI_MessageLoop::PushMessageLoop(gov);
gov->OnOpen();
GR_Main::CaptureSavePreview();
fGalaxy2::CaptureGalaxyPreview(gov);
GlobalsV::SaveManagerReturnScreenId = GlobalsV::FormToId(gov);
std::puts("SAVE_PREFLIGHT original_Gov_and_previews=PASS");
const auto path = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"Save\\InventoryProbe.sav"});
const bool accepted = aSaveLoad::SaveGameToFile(path, u"isolated Gov preflight"_w);
if (accepted) {
 aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
 std::puts("SAVE_PREFLIGHT original_save=ACCEPTED");
} else {
 std::puts("SAVE_PREFLIGHT original_save=REJECTED");
 // Never dereference the opaque script value: compare it only with objects
 // already owned by the live galaxy, then ask the original IdToShip inverse.
 for (int si = 0; si < pas::list_count(aGalaxy::Galaxy->Scripts); ++si) {
  auto* script = pas::list_at<aScript::TScript>(aGalaxy::Galaxy->Scripts, si);
  if (script->ScriptFileName != u"Script.PC_fem_rangers"_w) continue;
  auto* vars = script->InitCode->LocalVar;
  for (int vi = 0; vi < vars->Count; ++vi) {
   auto* cell = EC_Expression::TVarArrayEC_GetItem(vars, vi);
   if (cell->Name == u"quest_item"_w && cell->Kind == EC_Expression::vkDword) {
    const auto value = cell->DwordValue;
    std::printf("SAVE_PREFLIGHT quest_item_opaque=%llx\n", static_cast<unsigned long long>(value));
    auto* slots = fEquipmentShop::TemporaryShopSlots;
    for (int j = 0; slots && j < pas::list_count(slots); ++j) {
     auto* slot = pas::list_at<fEquipmentShop::TShopSlot>(slots, j);
     if (slot && slot->Item && reinterpret_cast<std::uintptr_t>(slot->Item) == value)
      std::printf("SAVE_PREFLIGHT quest_item_temporary_shop_match id=%u inverse=%d\n", unsigned(slot->Item->Id),
                  aGalaxy::Galaxy->IdToItem(unsigned(slot->Item->Id), false) == slot->Item);
    }
    for (int ti = 0; ti < pas::list_count(aGalaxy::Galaxy->Stars); ++ti) {
     auto* star = pas::list_at<aGalaxy::TStar>(aGalaxy::Galaxy->Stars, ti);
     for (int j = 0; star->MovingDropItems && j < pas::list_count(star->MovingDropItems); ++j) {
      auto* drop = pas::list_at<aGalaxy::TMovingDropItemEntry>(star->MovingDropItems, j);
      if (!drop || reinterpret_cast<std::uintptr_t>(drop->Payload) != value) continue;
      if (auto* item = pas::class_cast_if<aItem::TItem*>(drop->Payload))
       std::printf("SAVE_PREFLIGHT quest_item_moving_drop_match id=%u inverse=%d\n", unsigned(item->Id),
                   aGalaxy::Galaxy->IdToItem(unsigned(item->Id), false) == item);
     }
    }
   }
   if (cell->Name != u"quest_ship"_w || cell->Kind != EC_Expression::vkDword) continue;
   const auto value = cell->DwordValue;
   std::printf("SAVE_PREFLIGHT quest_ship_opaque=%llx\n", static_cast<unsigned long long>(value));
   for (int ti = 0; ti < pas::list_count(aGalaxy::Galaxy->Stars); ++ti) {
    auto* star = pas::list_at<aGalaxy::TStar>(aGalaxy::Galaxy->Stars, ti);
    for (int j = 0; star->Ships && j < pas::list_count(star->Ships); ++j) {
     auto* ship = pas::list_at<aShip::TShip>(star->Ships, j);
     if (reinterpret_cast<std::uintptr_t>(ship) == value)
      std::printf("SAVE_PREFLIGHT quest_ship_star_match id=%u inverse=%d\n", unsigned(ship->Id),
                  aGalaxy::Galaxy->IdToShip(unsigned(ship->Id), false) == ship);
    }
    for (int pi = 0; star->Planets && pi < pas::list_count(star->Planets); ++pi) {
     auto* planet = pas::list_at<aPlanet::TPlanet>(star->Planets, pi);
     for (int j = 0; planet->Warriors && j < pas::list_count(planet->Warriors); ++j) {
      auto* ship = pas::list_at<aShip::TShip>(planet->Warriors, j);
      if (reinterpret_cast<std::uintptr_t>(ship) == value)
       std::printf("SAVE_PREFLIGHT quest_ship_warrior_match id=%u inverse=%d\n", unsigned(ship->Id),
                   aGalaxy::Galaxy->IdToShip(unsigned(ship->Id), false) == ship);
     }
    }
   }
  }
 }
}
gov->OnClose();
GI_MessageLoop::PopMessageLoop(gov);
if (!previous_stack) {
 assert(pas::list_count(GI_MessageLoop::MessageLoopStack) == 0);
 pas::free(GI_MessageLoop::MessageLoopStack);
 GI_MessageLoop::MessageLoopStack = nullptr;
}
GlobalsV::CurrentScreenId = previous_screen_id;
