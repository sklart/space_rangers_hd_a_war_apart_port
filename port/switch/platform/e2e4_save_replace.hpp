#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

namespace srhd_awa::platform::e2e4_save_replace {

enum class Status {
    success,
    invalid_path,
    missing_or_empty_temp,
    target_check_failed,
    backup_slot_unavailable,
    backup_rename_failed,
    install_rename_failed,
    rollback_failed,
};

inline const char* StatusText(Status status) {
    switch (status) {
        case Status::success: return "success";
        case Status::invalid_path: return "invalid path";
        case Status::missing_or_empty_temp: return "missing or empty temp";
        case Status::target_check_failed: return "target check failed";
        case Status::backup_slot_unavailable: return "backup slot unavailable";
        case Status::backup_rename_failed: return "backup rename failed";
        case Status::install_rename_failed: return "install rename failed; old save restored";
        case Status::rollback_failed: return "install and rollback failed; old save remains in backup";
    }
    return "unknown";
}

inline std::filesystem::path Path(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return std::filesystem::path(value).lexically_normal();
}

// The temp file must already be fully written and closed. Rename operations
// are injectable so the otherwise rare failed-install path can be tested.
template <class Rename>
Status ReplaceWithBackup(const std::filesystem::path& source,
                         const std::filesystem::path& target, Rename&& rename) {
    namespace fs = std::filesystem;
    if (source.empty() || target.empty() || source == target ||
        source.parent_path() != target.parent_path() ||
        source.filename().empty() || target.filename().empty())
        return Status::invalid_path;
    std::error_code error;
    if (!fs::is_regular_file(source, error) || error ||
        fs::file_size(source, error) == 0 || error)
        return Status::missing_or_empty_temp;
    if (fs::is_symlink(fs::symlink_status(source, error)) || error)
        return Status::invalid_path;
    const bool old_exists = fs::exists(target, error);
    if (error || (old_exists && !fs::is_regular_file(target, error)) || error)
        return Status::target_check_failed;
    if (old_exists && (fs::is_symlink(fs::symlink_status(target, error)) || error))
        return Status::target_check_failed;
    fs::path backup;
    if (old_exists) {
        // Find an unused backup name. This helper never deletes a prior save
        // or backup, including when an earlier interrupted attempt left one.
        for (unsigned slot = 0; slot < 1024; ++slot) {
            backup = target;
            backup += slot == 0 ? ".bak" : ".bak." + std::to_string(slot);
            error.clear();
            const auto backup_status = fs::symlink_status(backup, error);
            if ((error == std::errc::no_such_file_or_directory ||
                 (!error && backup_status.type() == fs::file_type::not_found)) &&
                source != backup) {
                error.clear();
                break;
            }
            if (error || slot == 1023) return Status::backup_slot_unavailable;
        }
        if (!rename(target, backup, error) || error)
            return Status::backup_rename_failed;
    }
    error.clear();
    if (!rename(source, target, error) || error) {
        if (old_exists) {
            error.clear();
            if (!rename(backup, target, error) || error)
                return Status::rollback_failed;
        }
        return Status::install_rename_failed;
    }
    return Status::success;
}

inline Status ReplaceWithBackup(const std::string& source,
                                const std::string& target) {
    auto rename = [](const std::filesystem::path& from,
                     const std::filesystem::path& to,
                     std::error_code& error) {
        std::filesystem::rename(from, to, error);
        return !error;
    };
    return ReplaceWithBackup(Path(source), Path(target), rename);
}

} // namespace srhd_awa::platform::e2e4_save_replace
