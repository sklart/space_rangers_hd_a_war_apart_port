#pragma once
#if defined(E2E5_HOST_SDL_WINDOW)
#include <cstdlib>
#include <fstream>
#include <filesystem>
// Included after the original fGalaxy2 headers, in its effective translation unit.
// Observation only: coordinates are projected by the original map transform.
namespace srhd_awa::platform::e2e515_host_galaxy_snapshot {
inline void Observe(fGalaxy2::TfGalaxy2* screen) {
    const char* destination = std::getenv("E2E515_GALAXY_SNAPSHOT");
    if (!destination || !screen->MapPanel || !aPlayer::GetPlayer()) return;
    const auto* player = aPlayer::GetPlayer();
    const std::filesystem::path path(destination), temporary = path.string() + ".tmp";
    std::ofstream out(temporary);
    out << "{\"turn\":" << aGalaxy::Galaxy->CurrentTurn
        << ",\"star\":" << (player->CurrentStar ? player->CurrentStar->Id : 0)
        << ",\"selected\":" << (screen->SelectedJumpStar ? screen->SelectedJumpStar->Id : 0)
        << ",\"jump_range\":" << player->JumpRange
        << ",\"jump_disabled\":" << (screen->JumpButton->Disabled ? "true" : "false")
        << ",\"stars\":[";
    bool first = true;
    for (int i = 0; i < pas::list_count(aGalaxy::Galaxy->Stars); ++i) {
        auto* star = pas::list_at<aGalaxy::TStar>(aGalaxy::Galaxy->Stars, i);
        if (!star->IsConstellationVisible() || !player->CurrentStar) continue;
        const auto point = screen->MapPanel->ToAbsolutePoint(screen->GalaxyPointToMapPoint(star->Position));
        const auto dx = static_cast<long double>(star->Position.X) - player->CurrentStar->Position.X;
        const auto dy = static_cast<long double>(star->Position.Y) - player->CurrentStar->Position.Y;
        const auto distance = System::Round(System::Sqrt(dx * dx + dy * dy));
        if (!first) out << ',';
        first = false;
        out << "{\"id\":" << star->Id << ",\"x\":" << point.X << ",\"y\":" << point.Y
            << ",\"distance\":" << distance << '}';
    }
    out << "]}\n"; out.close();
    if (!out) throw std::runtime_error("galaxy observation write failed");
    std::filesystem::rename(temporary, path);
}
}
#endif
