// Exercise the real save preflight on a generated campaign in an isolated root.
const auto path = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"Save\\InventoryProbe.sav"});
const bool accepted = aSaveLoad::SaveGameToFile(path, u"isolated preflight"_w);
if (accepted) {
 aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
 std::puts("SAVE_PREFLIGHT original_save=ACCEPTED");
} else {
 std::puts("SAVE_PREFLIGHT original_save=REJECTED");
}
