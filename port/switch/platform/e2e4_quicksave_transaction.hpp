#pragma once

#include <cstdint>
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace srhd_awa::platform::e2e4_quicksave {

// The game has one serialized TSaver queue. File existence alone cannot
// distinguish a failed replacement from the occupied slot's previous save.
inline std::atomic<bool> game_save_committed{false};
inline void BeginGameSave() { game_save_committed.store(false, std::memory_order_release); }
inline void CompleteGameSave() { game_save_committed.store(true, std::memory_order_release); }
inline bool GameSaveCommitted() { return game_save_committed.load(std::memory_order_acquire); }

namespace fs = std::filesystem;

// The callback returns false to simulate a failed operation. It is called
// before every filesystem mutation and at each durable stage boundary.
using Fault = std::function<bool(const char*)>;

struct Result {
    bool ok{};
    bool recovered{};
    fs::path backup_directory;
    std::string detail;
};

// The source is a closed save.tmp; slots are one-based in rotation order.
// No existing backup directory is ever removed by this module.
Result Rotate(const fs::path& source, const std::vector<fs::path>& slots,
              const Fault& fault = {});
Result Recover(const fs::path& save_directory, const Fault& fault = {});
bool ValidateSave(const fs::path& source, std::string& detail);
Result RotateGamePaths(const char16_t* source, const char16_t* quick_slot_one,
                       std::int32_t extra_slots);
Result RecoverGamePaths(const char16_t* quick_slot_one);
bool CanCreateGameTemp(const char16_t* source);
bool GameSavePathExists(const char16_t* path);
Result ReplaceGameSavePath(const char16_t* source, const char16_t* target);

} // namespace srhd_awa::platform::e2e4_quicksave
