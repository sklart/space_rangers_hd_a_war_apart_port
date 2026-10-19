#pragma once
#include <mutex>
#include <string>
#include <string_view>

namespace srhd_awa::platform::pending_game_load {
// PendingLoadFileName is a legacy ANSI global also used by quick/autoload.
// Keep its ABI, but hand the save manager's original Unicode filename to the
// load worker. The ANSI bytes identify the request; they are never decoded
// back into the user-selected path.
inline std::mutex mutex;
inline std::u16string filename;
inline std::string legacy_request;

inline void Store(const char16_t* wide, std::string_view legacy) {
    std::lock_guard lock(mutex);
    filename = wide ? wide : u"";
    legacy_request = legacy;
}

inline std::u16string Take(std::string_view legacy) {
    std::lock_guard lock(mutex);
    std::u16string result;
    if (legacy == legacy_request) result = std::move(filename);
    filename.clear();
    legacy_request.clear();
    return result;
}
}  // namespace srhd_awa::platform::pending_game_load
