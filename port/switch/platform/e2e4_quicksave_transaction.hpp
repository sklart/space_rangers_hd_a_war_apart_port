#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace srhd_awa::platform::e2e4_quicksave {

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
Result ReplaceGameSavePath(const char16_t* source, const char16_t* target);

} // namespace srhd_awa::platform::e2e4_quicksave
