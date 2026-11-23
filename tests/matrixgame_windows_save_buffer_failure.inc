// Deliberately fail the real save at two ownership boundaries in an isolated root.
const auto path = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"Save\\BufferProbe.sav"});
const auto local_file = std::filesystem::path(output_root) / "rangers-user/config/save/BufferProbe.sav";
auto read_file = [&] {
 std::ifstream stream(local_file, std::ios::binary);
 assert(stream);
 return std::vector<char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
};
const auto before_success = matrixgame_save_buffer_probe::live.load();
assert(aSaveLoad::SaveGameToFile(path, u"isolated success"_w));
aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
const auto original_file = read_file(); assert(!original_file.empty());
const auto success_delta = matrixgame_save_buffer_probe::live.load() - before_success;
std::printf("SAVE_BUFFER_SUCCESS live_delta=%ld\n", success_delta);
assert(success_delta == 0);
for (int point : {1, 2}) {
 const auto before = matrixgame_save_buffer_probe::live.load();
 matrixgame_save_buffer_probe::fail_before = point;
 const bool accepted = aSaveLoad::SaveGameToFile(path, u"isolated failure"_w);
 matrixgame_save_buffer_probe::fail_before = 0;
 aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
 const auto delta = matrixgame_save_buffer_probe::live.load() - before;
 assert(!accepted);
 assert(read_file() == original_file);
 std::printf("SAVE_BUFFER_FAILURE point=%d live_delta=%ld\n", point, delta);
 if (std::getenv("SRHD_EXPECT_CLEAN_BUFFERS")) assert(delta == 0);
}
if (std::getenv("SRHD_EXPECT_CLEAN_BUFFERS")) {
 const auto id = aPlayer::GetPlayer()->Id;
 const auto turn = aGalaxy::Galaxy->CurrentTurn;
 const auto money = aPlayer::GetPlayer()->Money;
 const auto before = matrixgame_save_buffer_probe::live.load();
 matrixgame_save_buffer_probe::fail_before = 3;
 assert(!aSaveLoad::SaveGameToFile(path, u"post-queue failure"_w));
 matrixgame_save_buffer_probe::fail_before = 0;
 aSaveLoad::SaveWriter->WaitForIdle(WindowsSdk::INFINITE);
 assert(matrixgame_save_buffer_probe::live.load() == before);
 assert(aSaveLoad::LoadGameFromFile(path));
 assert(aPlayer::GetPlayer()->Id == id && aPlayer::GetPlayer()->Money == money && aGalaxy::Galaxy->CurrentTurn == turn);
 std::puts("SAVE_BUFFER_AFTER_QUEUE worker_ownership_and_reload=PASS");
}
