#include "e2e4_quicksave_transaction.hpp"
#include "e2e4_save_replace.hpp"
#include "unicode_path.hpp"
#include "user_root.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdio.h>
#include <system_error>
#include <vector>

#include <zlib.h>

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace srhd_awa::platform::e2e4_quicksave {
namespace {
constexpr unsigned kMaxTransactions = 16;
constexpr std::uint64_t kMaxSaveBytes = 1024ull * 1024 * 1024;
constexpr std::uint64_t kMaxStateBytes = 512ull * 1024 * 1024;

bool Step(const Fault& fault, const char* name) { return !fault || fault(name); }

enum class Kind { missing, regular, directory, unsafe };
Kind FileKind(const fs::path& path) {
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) return Kind::missing;
    if (ec) return Kind::unsafe;
    if (fs::is_symlink(status)) return Kind::unsafe;
    if (fs::is_regular_file(status)) return Kind::regular;
    if (fs::is_directory(status)) return Kind::directory;
    if (status.type() == fs::file_type::not_found) return Kind::missing;
    return Kind::unsafe;
}

bool SafeDirectory(const fs::path& path) {
    if (path.empty() || FileKind(path) != Kind::directory) return false;
    for (auto part = path.begin(); part != path.end(); ++part)
        if (*part == "..") return false;
    fs::path cursor;
    for (const auto& part : path) {
        cursor /= part;
        if (cursor.empty() || cursor == "/") continue;
        // Some Switch VFS root components (sdmc:) are virtual, but all
        // existing real path components must be directories, never links.
        const auto kind = FileKind(cursor);
        if (kind == Kind::unsafe || kind == Kind::regular) return false;
    }
    return true;
}

struct Fingerprint {
    std::uint64_t size{};
    std::uint32_t crc{};
    bool operator==(const Fingerprint&) const = default;
};

bool HashFile(const fs::path& path, Fingerprint& result) {
    if (FileKind(path) != Kind::regular) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::array<char, 32768> buffer{};
    result = {};
    auto crc = crc32(0, nullptr, 0);
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) {
            result.size += static_cast<std::uint64_t>(count);
            if (result.size > kMaxSaveBytes) return false;
            crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()),
                        static_cast<uInt>(count));
        }
    }
    if (!input.eof() || input.bad()) return false;
    result.crc = static_cast<std::uint32_t>(crc);
    return true;
}

bool SyncFile(const fs::path& path) {
#ifdef _WIN32
    FILE* file = _wfopen(path.c_str(), L"rb+");
    if (!file) return false;
    const bool ok = _commit(_fileno(file)) == 0;
#else
    FILE* file = std::fopen(path.c_str(), "rb+");
    if (!file) return false;
    const bool ok = fsync(fileno(file)) == 0;
#endif
    return std::fclose(file) == 0 && ok;
}

// libstdc++ copy_file also copies permission metadata. libnx's fchmod
// returns ENOSYS, leaving an empty destination before any bytes are copied.
// On POSIX use only byte I/O; exclusive creation retains no-overwrite safety.
// CopyVerified still flushes and reads back the complete size/CRC afterwards.
bool CopyBytesExclusive(const fs::path& from, const fs::path& to,
                        std::error_code& ec) {
#ifdef _WIN32
    return fs::copy_file(from, to, fs::copy_options::none, ec);
#else
    FILE* input = std::fopen(from.c_str(), "rb");
    if (!input) { ec = {errno, std::generic_category()}; return false; }
    const int fd = ::open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        ec = {errno, std::generic_category()};
        std::fclose(input);
        return false;
    }
    FILE* output = fdopen(fd, "wb");
    if (!output) {
        ec = {errno, std::generic_category()};
        ::close(fd);
        std::fclose(input);
        return false;
    }
    auto failed = [&] {
        if (!ec) ec = {errno ? errno : EIO, std::generic_category()};
    };
    std::array<unsigned char, 32768> buffer{};
    std::uint64_t total{};
    while (!ec) {
        const auto count = std::fread(buffer.data(), 1, buffer.size(), input);
        if (count) {
            total += count;
            if (total > kMaxSaveBytes) { ec = {EFBIG, std::generic_category()}; break; }
            if (std::fwrite(buffer.data(), 1, count, output) != count) { failed(); break; }
        }
        if (std::ferror(input)) { failed(); break; }
        if (std::feof(input)) break;
        if (!count) { ec = {EIO, std::generic_category()}; break; }
    }
    if (std::fflush(output) != 0) failed();
    if (std::fclose(output) != 0) failed();
    if (std::fclose(input) != 0) failed();
    return !ec;
#endif
}

bool CopyVerified(const fs::path& from, const fs::path& to,
                  const Fingerprint& expected, const Fault& fault,
                  const char* operation, std::string& error) {
    if (FileKind(from) != Kind::regular || FileKind(to) != Kind::missing) {
        error = std::string(operation) + ": unsafe source or occupied destination";
        return false;
    }
    if (!Step(fault, operation)) { error = std::string(operation) + ": injected failure"; return false; }
    std::error_code ec;
    if (!CopyBytesExclusive(from, to, ec) || ec ||
        FileKind(to) != Kind::regular) {
        error = std::string(operation) + ": byte copy failed: " + ec.message() +
                ", errno=" + std::strerror(errno);
        return false;
    }
    if (!Step(fault, "copy-before-flush") || !SyncFile(to)) {
        error = std::string(operation) + ": interrupted write or flush failed";
        return false;
    }
    Fingerprint actual;
    if (!HashFile(to, actual) || actual != expected) {
        error = std::string(operation) + ": copied bytes failed size/CRC check";
        return false;
    }
    return true;
}

bool RemoveVerified(const fs::path& path, const Fingerprint& expected,
                    const Fault& fault, const char* operation, std::string& error) {
    Fingerprint actual;
    if (!HashFile(path, actual) || actual != expected) {
        error = std::string(operation) + ": source changed or unreadable";
        return false;
    }
    if (!Step(fault, operation)) { error = std::string(operation) + ": injected failure"; return false; }
    std::error_code ec;
    if (!fs::remove(path, ec) || ec) {
        error = std::string(operation) + ": " + ec.message();
        return false;
    }
    return true;
}

bool WriteMarker(const fs::path& path, const std::string& text,
                 const Fault& fault, const char* operation, std::string& error) {
    if (FileKind(path) != Kind::missing || !Step(fault, operation)) {
        error = std::string(operation) + ": destination occupied or injected failure";
        return false;
    }
#ifdef _WIN32
    FILE* file = _wfopen(path.c_str(), L"wb");
#else
    FILE* file = std::fopen(path.c_str(), "wb");
#endif
    if (!file) { error = std::string(operation) + ": open failed"; return false; }
    const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size() &&
                         std::fflush(file) == 0;
#ifdef _WIN32
    const bool synced = written && _commit(_fileno(file)) == 0;
#else
    const bool synced = written && fsync(fileno(file)) == 0;
#endif
    const bool closed = std::fclose(file) == 0;
    if (!written || !synced || !closed) {
        error = std::string(operation) + ": write, flush or close failed";
        return false;
    }
    return true;
}

struct Entry { bool present{}; Fingerprint fingerprint{}; };
struct Manifest { std::vector<Entry> entries; };

fs::path Original(const fs::path& transaction, std::size_t index) {
    return transaction / ("slot" + std::to_string(index + 1) + ".orig");
}
fs::path Interrupted(const fs::path& transaction, std::size_t index) {
    return transaction / ("slot" + std::to_string(index + 1) + ".interrupted");
}
fs::path SlotPath(const fs::path& save_directory, std::size_t index) {
    return save_directory / (index == 0 ? "QuickSave.sav" :
                             "QuickSave" + std::to_string(index + 1) + ".sav");
}

fs::path GamePath(const char16_t* wide) {
    if (!wide) return {};
    auto utf8 = PathUtf8(wide);
    for (char& c : utf8) if (c == '\\') c = '/';
    const auto config = user_root::ConfigDirectory();
    if (!config.empty() && utf8.starts_with(config))
        utf8 = user_root::ResolveConfigPath(utf8);
    return fs::u8path(utf8).lexically_normal();
}

bool LoadManifest(const fs::path& transaction, Manifest& manifest) {
    if (FileKind(transaction / "manifest") != Kind::regular) return false;
    std::ifstream stream(transaction / "manifest", std::ios::binary);
    std::string version;
    std::size_t count{};
    if (!(stream >> version >> count) || version != "E2E4-QUICKSAVE-1" ||
        count == 0 || count > 32) return false;
    manifest.entries.resize(count);
    for (auto& entry : manifest.entries) {
        unsigned present{};
        if (!(stream >> present >> entry.fingerprint.size >> entry.fingerprint.crc) || present > 1)
            return false;
        entry.present = present != 0;
    }
    std::string extra;
    return !(stream >> extra);
}

std::string ManifestText(const Manifest& manifest) {
    std::ostringstream out;
    out << "E2E4-QUICKSAVE-1 " << manifest.entries.size() << '\n';
    for (const auto& entry : manifest.entries)
        out << (entry.present ? 1 : 0) << ' ' << entry.fingerprint.size << ' '
            << entry.fingerprint.crc << '\n';
    return out.str();
}

bool MarkerIs(const fs::path& path, const char* expected) {
    if (FileKind(path) != Kind::regular) return false;
    std::ifstream file(path, std::ios::binary);
    std::string content(std::istreambuf_iterator<char>{file}, {});
    return !file.bad() && content == expected;
}

Result Rollback(const fs::path& save_directory, const fs::path& transaction,
                const Manifest& manifest, const Fault& fault) {
    std::string error;
    for (std::size_t i = 0; i < manifest.entries.size(); ++i) {
        const auto target = SlotPath(save_directory, i);
        const auto backup = Original(transaction, i);
        const auto quarantine_base = Interrupted(transaction, i);
        const auto& entry = manifest.entries[i];
        if (entry.present) {
            Fingerprint snapshot;
            if (!HashFile(backup, snapshot) || snapshot != entry.fingerprint)
                return {false, false, transaction, "rollback: snapshot missing or damaged: " + backup.string()};
        } else if (FileKind(backup) != Kind::missing) {
            return {false, false, transaction, "rollback: unexpected snapshot: " + backup.string()};
        }
        const auto kind = FileKind(target);
        if (kind == Kind::unsafe || kind == Kind::directory)
            return {false, false, transaction, "rollback: unsafe slot: " + target.string()};
        if (kind == Kind::regular) {
            Fingerprint current;
            if (!HashFile(target, current))
                return {false, false, transaction, "rollback: unreadable slot: " + target.string()};
            if (entry.present && current == entry.fingerprint) continue;
            fs::path quarantine;
            bool already_copied = false;
            for (unsigned copy = 0; copy < 16; ++copy) {
                auto candidate = quarantine_base;
                if (copy > 0) candidate += "." + std::to_string(copy);
                const auto candidate_kind = FileKind(candidate);
                if (candidate_kind == Kind::unsafe || candidate_kind == Kind::directory)
                    return {false, false, transaction, "rollback: unsafe quarantine: " + candidate.string()};
                if (candidate_kind == Kind::missing) { quarantine = candidate; break; }
                Fingerprint prior;
                if (!HashFile(candidate, prior))
                    return {false, false, transaction, "rollback: unreadable quarantine: " + candidate.string()};
                if (prior == current) {
                    quarantine = candidate;
                    already_copied = true;
                    break;
                }
            }
            if (quarantine.empty())
                return {false, false, transaction, "rollback: interrupted-file limit reached for " + target.string()};
            if (!already_copied && !CopyVerified(target, quarantine, current, fault,
                                                 "quarantine-copy", error))
                return {false, false, transaction, "rollback: " + error};
            if (!RemoveVerified(target, current, fault, "quarantine-remove", error))
                return {false, false, transaction, "rollback: " + error};
        }
        if (entry.present && !CopyVerified(backup, target, entry.fingerprint, fault,
                                           "restore-copy", error))
            return {false, false, transaction, "rollback: " + error};
    }
    if (!WriteMarker(transaction / "rolled-back", "restored\n", fault,
                     "rollback-marker", error))
        return {false, false, transaction, "rollback: " + error};
    return {true, true, transaction, "original quicksave slots restored; backups retained"};
}

std::string Utf8Name(const fs::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
struct ManualManifest { fs::path target, backup; Fingerprint previous, next; };
bool SafeName(const fs::path& name) {
    const auto text = Utf8Name(name);
    return !text.empty() && text.size() <= 4096 && name == name.filename() &&
        text != "." && text != ".." && text.find_first_of("/\\:") == std::string::npos;
}
std::string ManualText(const ManualManifest& value) {
    std::ostringstream out;
    out << "E2E4-MANUAL-1\n" << std::quoted(Utf8Name(value.target)) << '\n'
        << std::quoted(Utf8Name(value.backup)) << '\n' << value.previous.size << ' '
        << value.previous.crc << ' ' << value.next.size << ' ' << value.next.crc << '\n';
    return out.str();
}
bool ReadManual(const fs::path& record, ManualManifest& value) {
    Fingerprint manifest;
    if (!HashFile(record / "manifest", manifest) || manifest.size > 16384) return false;
    std::ifstream file(record / "manifest", std::ios::binary);
    std::string version, target, backup, extra;
    if (!(file >> version >> std::quoted(target) >> std::quoted(backup) >> value.previous.size
          >> value.previous.crc >> value.next.size >> value.next.crc) ||
        version != "E2E4-MANUAL-1" || (file >> extra)) return false;
    value.target = fs::u8path(target); value.backup = fs::u8path(backup);
    if (!SafeName(value.target) || value.target.extension() != ".sav" ||
        value.target == "QuickSave.sav" || !value.next.size || value.next.size > kMaxSaveBytes ||
        value.previous.size > kMaxSaveBytes) return false;
    if (backup.empty()) return value.previous.size == 0 && value.previous.crc == 0;
    const auto prefix = target + ".bak";
    if (!SafeName(value.backup) || !backup.starts_with(prefix)) return false;
    if (backup == prefix) return true;
    const auto suffix = backup.substr(prefix.size());
    return suffix.size() > 1 && suffix[0] == '.' &&
        std::all_of(suffix.begin() + 1, suffix.end(), [](char c) { return c >= '0' && c <= '9'; });
}
// Remove only our small terminal metadata. Interrupted save bytes stay retained.
bool CleanManual(const fs::path& record) {
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(record, error)) {
        const auto name = entry.path().filename().string();
        if (name != "manifest" && name != "ready" && name != "committed" && name != "restored") return true;
        if (FileKind(entry.path()) != Kind::regular) return false;
    }
    if (error) return false;
    // Keep the terminal marker until the manifest is gone: a process may stop
    // during cleanup, and recovery must never roll back a committed generation.
    for (const auto* name : {"manifest", "ready", "committed", "restored"}) {
        const auto path = record / name;
        if (FileKind(path) == Kind::missing) continue;
        Fingerprint fingerprint; std::string detail;
        if (!HashFile(path, fingerprint) || !RemoveVerified(path, fingerprint, {}, "manual-clean-metadata", detail)) return false;
    }
    return fs::remove(record, error) && !error;
}
Result RecoverManual(const fs::path& directory, const Fault& fault = {}) {
    const auto root = directory / ".e2e4-manual-transactions";
    if (FileKind(root) == Kind::missing) return {true, false, {}, "no manual transactions"};
    if (!SafeDirectory(root)) return {false, false, root, "unsafe manual recovery directory"};
    bool recovered = false;
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(root, error)) {
        const auto record = entry.path();
        if (!SafeDirectory(record) || !record.filename().string().starts_with("txn-"))
            return {false, recovered, record, "unexpected manual recovery entry"};
        if (MarkerIs(record / "committed", "complete\n") || MarkerIs(record / "restored", "restored\n")) {
            CleanManual(record); continue;
        }
        // No target is mutated before both manifest and ready are durable.
        if (!MarkerIs(record / "ready", "prepared\n")) {
            // Unknown files in our namespace are retained, not treated as ours.
            ManualManifest unprepared;
            if (FileKind(record / "manifest") == Kind::missing || ReadManual(record, unprepared))
                CleanManual(record);
            continue;
        }
        ManualManifest manifest;
        if (!ReadManual(record, manifest)) return {false, recovered, record, "invalid manual recovery manifest; files retained"};
        const auto target = directory / manifest.target;
        const bool occupied = !manifest.backup.empty();
        Fingerprint actual;
        if (occupied && (!HashFile(directory / manifest.backup, actual) || actual != manifest.previous))
            return {false, recovered, record, "manual original backup unavailable; files retained"};
        const auto kind = FileKind(target);
        if (kind != Kind::regular && kind != Kind::missing)
            return {false, recovered, record, "unsafe manual destination during recovery"};
        bool original_present = false;
        std::string detail;
        if (kind == Kind::regular) {
            if (!HashFile(target, actual)) return {false, recovered, record, "manual destination unreadable"};
            original_present = occupied && actual == manifest.previous;
            if (!original_present) {
                fs::path retained;
                Fingerprint temp_fingerprint;
                const auto temp = directory / "save.tmp";
                if (actual == manifest.next && HashFile(temp, temp_fingerprint) && temp_fingerprint == actual)
                    retained = temp; // Already retained; rollback needs no additional save-sized allocation.
                for (unsigned i = 0; retained.empty() && i < 1024; ++i) {
                    const auto candidate = record / ("interrupted-" + std::to_string(i) + ".tmp");
                    Fingerprint prior;
                    if (HashFile(candidate, prior) && prior == actual) { retained = candidate; break; }
                    if (FileKind(candidate) == Kind::missing) {
                        if (!CopyVerified(target, candidate, actual, fault, "manual-quarantine", detail))
                            return {false, recovered, record, detail};
                        retained = candidate; break;
                    }
                }
                if (retained.empty() || !RemoveVerified(target, actual, fault, "manual-remove-interrupted", detail))
                    return {false, recovered, record, "manual interrupted bytes retained: " + detail};
            }
        }
        if (occupied && !original_present && !CopyVerified(directory / manifest.backup, target, manifest.previous,
                                                           fault, "manual-restore-old", detail))
            return {false, recovered, record, detail + "; old backup retained"};
        if (!WriteMarker(record / "restored", "restored\n", fault, "manual-restored-marker", detail))
            return {false, recovered, record, detail};
        recovered = true;
        CleanManual(record);
    }
    if (error) return {false, recovered, root, "manual recovery enumeration failed"};
    return {true, recovered, root, recovered ? "interrupted manual save restored" : "no pending manual save"};
}

bool ReadU16String(std::ifstream& stream, std::uint64_t& remaining,
                   const char* expected, std::string& error,
                   std::string* actual = nullptr) {
    std::string ascii;
    for (unsigned i = 0; i < 2048; ++i) {
        if (remaining < 2) { error = "truncated UTF-16 header"; return false; }
        unsigned char bytes[2]{};
        if (!stream.read(reinterpret_cast<char*>(bytes), 2)) { error = "header read failed"; return false; }
        remaining -= 2;
        const auto code = static_cast<unsigned>(bytes[0]) | (static_cast<unsigned>(bytes[1]) << 8);
        if (code == 0) {
            if (actual) *actual = ascii;
            return !expected || ascii == expected;
        }
        if (expected || actual) {
            if (code > 127) { error = "signature is not ASCII"; return false; }
            ascii.push_back(static_cast<char>(code));
        }
    }
    error = "unterminated or oversized UTF-16 header";
    return false;
}

bool ReadU32(std::ifstream& stream, std::uint64_t& remaining, std::uint32_t& value) {
    if (remaining < 4) return false;
    unsigned char bytes[4]{};
    if (!stream.read(reinterpret_cast<char*>(bytes), 4)) return false;
    remaining -= 4;
    value = static_cast<std::uint32_t>(bytes[0]) |
            (static_cast<std::uint32_t>(bytes[1]) << 8) |
            (static_cast<std::uint32_t>(bytes[2]) << 16) |
            (static_cast<std::uint32_t>(bytes[3]) << 24);
    return true;
}

} // namespace

bool ValidateSave(const fs::path& source, std::string& detail) {
    Fingerprint whole;
    if (!HashFile(source, whole) || whole.size < 64) {
        detail = "save.tmp missing, unreadable, empty or too large";
        return false;
    }
    std::ifstream input(source, std::ios::binary);
    if (!input) { detail = "save.tmp open failed"; return false; }
    std::uint64_t remaining = whole.size;
    std::string signature, version;
    for (unsigned i = 0; i < 8; ++i) {
        const char* expected = i == 7 ? "EZ" : nullptr;
        std::string* actual = i == 0 ? &signature : (i == 1 ? &version : nullptr);
        if (!ReadU16String(input, remaining, expected, detail, actual)) {
            if (detail.empty()) detail = "save header signature mismatch";
            return false;
        }
    }
    if (signature != "RSG" && !(signature == "RSG64" && version == "v168")) {
        detail = "save header signature or typed version mismatch";
        return false;
    }
    for (unsigned i = 0; i < 2; ++i) {
        std::uint32_t size{};
        if (!ReadU32(input, remaining, size) || size > remaining) {
            detail = "preview block size exceeds file"; return false;
        }
        input.seekg(static_cast<std::streamoff>(size), std::ios::cur);
        if (!input) { detail = "preview block seek failed"; return false; }
        remaining -= size;
    }
    std::uint32_t saved_crc{}, seed{}, state_size{};
    if (!ReadU32(input, remaining, saved_crc) || !ReadU32(input, remaining, seed) ||
        !ReadU32(input, remaining, state_size) || state_size < 8 ||
        state_size > remaining || state_size > kMaxStateBytes) {
        detail = "game-state block size invalid"; return false;
    }
    z_stream z{};
    if (inflateInit(&z) != Z_OK) { detail = "zlib initialization failed"; return false; }
    std::array<unsigned char, 32768> input_bytes{}, output_bytes{};
    auto crc = crc32(0, nullptr, 0);
    std::int32_t state = static_cast<std::int32_t>(seed);
    std::uint64_t expanded{};
    auto left = static_cast<std::uint64_t>(state_size);
    bool ended = false;
    bool valid = true;
    while (left > 0 && valid) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(left, input_bytes.size()));
        if (!input.read(reinterpret_cast<char*>(input_bytes.data()), static_cast<std::streamsize>(count))) {
            detail = "game-state read failed"; valid = false; break;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto next = 16807 * (state % 127773) - 2836 * (state / 127773);
            state = next <= 0 ? next + 0x7fffffff : next;
            input_bytes[i] ^= static_cast<unsigned char>(state - 1);
        }
        crc = crc32(crc, input_bytes.data(), static_cast<uInt>(count));
        z.next_in = input_bytes.data();
        z.avail_in = static_cast<uInt>(count);
        while (z.avail_in > 0 && valid) {
            z.next_out = output_bytes.data();
            z.avail_out = static_cast<uInt>(output_bytes.size());
            const auto rc = inflate(&z, Z_NO_FLUSH);
            expanded += output_bytes.size() - z.avail_out;
            if (expanded > kMaxStateBytes || (rc != Z_OK && rc != Z_STREAM_END)) {
                detail = "game-state zlib payload invalid"; valid = false; break;
            }
            if (rc == Z_STREAM_END) {
                ended = true;
                if (z.avail_in || left != count) {
                    detail = "trailing bytes inside game-state block"; valid = false;
                }
                break;
            }
        }
        left -= count;
    }
    inflateEnd(&z);
    if (!valid || !ended || expanded == 0 || static_cast<std::uint32_t>(crc) != saved_crc) {
        if (detail.empty()) detail = "game-state zlib or CRC32 check failed";
        return false;
    }
    remaining -= state_size;
    // The optional film-history tail is written after the checked state block.
    // The original loader does not store an independent checksum for it.
    detail = "RSG/EZ structure, block bounds, zlib stream and state CRC32 verified";
    return true;
}

Result Recover(const fs::path& save_directory, const Fault& fault) {
    if (!SafeDirectory(save_directory)) return {false, false, {}, "unsafe save directory"};
    const auto manual = RecoverManual(save_directory, fault);
    if (!manual.ok) return manual;
    const auto root = save_directory / ".e2e4-quicksave-backups";
    if (FileKind(root) == Kind::missing) return manual;
    if (FileKind(root) != Kind::directory || !SafeDirectory(root))
        return {false, false, root, "unsafe backup root"};
    bool recovered = manual.recovered;
    std::error_code ec;
    for (const auto& item : fs::directory_iterator(root, ec)) {
        if (ec || FileKind(item.path()) != Kind::directory ||
            item.path().filename().string().rfind("txn-", 0) != 0)
            return {false, recovered, item.path(), "unexpected backup entry; manual inspection required"};
        const auto transaction = item.path();
        Manifest manifest;
        if (!LoadManifest(transaction, manifest))
            return {false, recovered, transaction,
                    "incomplete or invalid manifest; original slots left untouched; inspect manually"};
        if (MarkerIs(transaction / "committed", "complete\n") ||
            MarkerIs(transaction / "rolled-back", "restored\n")) continue;
        if (FileKind(transaction / "committed") != Kind::missing ||
            FileKind(transaction / "rolled-back") != Kind::missing)
            return {false, recovered, transaction, "invalid transaction marker; inspect snapshots manually"};
        const auto result = Rollback(save_directory, transaction, manifest, fault);
        if (!result.ok) return result;
        recovered = true;
    }
    if (ec) return {false, recovered, root, "backup enumeration failed: " + ec.message()};
    return {true, recovered, root, recovered ? "interrupted rotation restored" : "no pending rotation"};
}

Result Rotate(const fs::path& source, const std::vector<fs::path>& slots,
              const Fault& fault) {
    if (slots.empty() || slots.size() > 32 || source.empty())
        return {false, false, {}, "invalid slot count or source"};
    const auto directory = source.parent_path();
    if (!SafeDirectory(directory) || FileKind(source) != Kind::regular)
        return {false, false, {}, "unsafe save path or source"};
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (slots[i] != SlotPath(directory, i) || slots[i] == source ||
            (FileKind(slots[i]) != Kind::regular && FileKind(slots[i]) != Kind::missing))
            return {false, false, {}, "unexpected quicksave path or unsafe slot"};
    auto recovery = Recover(directory);
    if (!recovery.ok) return recovery;
    std::string detail;
    if (!Step(fault, "read-new-save"))
        return {false, recovery.recovered, {}, "save.tmp read fault; original slots unchanged"};
    if (!SyncFile(source))
        return {false, recovery.recovered, {}, "save.tmp flush failed; original slots unchanged"};
    if (!ValidateSave(source, detail)) return {false, recovery.recovered, {}, detail};
    Fingerprint new_save;
    if (!HashFile(source, new_save)) return {false, recovery.recovered, {}, "save.tmp read failed"};
    const auto root = directory / ".e2e4-quicksave-backups";
    std::error_code ec;
    if (FileKind(root) == Kind::missing) {
        if (!Step(fault, "create-backup-root") || !fs::create_directory(root, ec) || ec)
            return {false, recovery.recovered, root, "could not create backup root"};
    } else if (FileKind(root) != Kind::directory || !SafeDirectory(root)) {
        return {false, recovery.recovered, root, "unsafe backup root"};
    }
    fs::path transaction;
    for (unsigned i = 1; i <= kMaxTransactions; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "txn-%02u", i);
        const auto candidate = root / name;
        if (FileKind(candidate) == Kind::missing) { transaction = candidate; break; }
    }
    if (transaction.empty()) return {false, recovery.recovered, root,
                                    "backup limit reached (16); no old files changed"};
    if (!Step(fault, "create-transaction") || !fs::create_directory(transaction, ec) || ec)
        return {false, recovery.recovered, transaction, "transaction directory creation failed"};
    Manifest manifest;
    manifest.entries.resize(slots.size());
    for (std::size_t i = 0; i < slots.size(); ++i) {
        auto& entry = manifest.entries[i];
        entry.present = FileKind(slots[i]) == Kind::regular;
        if (entry.present) {
            if (!Step(fault, "read-original-slot"))
                return {false, recovery.recovered, transaction, "original slot read fault; old files unchanged"};
            if (!HashFile(slots[i], entry.fingerprint))
                return {false, recovery.recovered, transaction, "original slot read failed"};
            if (!CopyVerified(slots[i], Original(transaction, i), entry.fingerprint,
                              fault, "snapshot-copy", detail))
                return {false, recovery.recovered, transaction, detail};
        }
    }
    if (!WriteMarker(transaction / "manifest", ManifestText(manifest), fault,
                     "write-manifest", detail))
        return {false, recovery.recovered, transaction, detail};

    auto fail = [&](const std::string& reason) -> Result {
        const auto rollback = Rollback(directory, transaction, manifest, fault);
        if (!rollback.ok)
            return {false, recovery.recovered, transaction,
                    reason + "; automatic rollback FAILED: " + rollback.detail +
                    "; snapshots and save.tmp retained"};
        return {false, true, transaction, reason + "; originals restored; save.tmp retained"};
    };
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (manifest.entries[i].present &&
            !RemoveVerified(slots[i], manifest.entries[i].fingerprint, fault,
                            "remove-after-snapshot", detail)) return fail(detail);
    if (!Step(fault, "all-originals-cleared")) return fail("interrupted after clearing originals");
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto from = i == 0 ? source : Original(transaction, i - 1);
        const auto expected = i == 0 ? new_save : manifest.entries[i - 1].fingerprint;
        if (i > 0 && !manifest.entries[i - 1].present) continue;
        if (!CopyVerified(from, slots[i], expected, fault, "publish-copy", detail))
            return fail(detail);
        if (!Step(fault, "published-slot")) return fail("interrupted after publishing slot");
    }
    const auto commit_marker = transaction / "committed";
    if (!WriteMarker(commit_marker, "complete\n", fault,
                     "commit-marker", detail)) {
        if (FileKind(commit_marker) != Kind::missing) {
            std::error_code marker_error;
            if (!fs::remove(commit_marker, marker_error) || marker_error)
                return {false, recovery.recovered, transaction,
                        detail + "; commit marker could not be cleared; snapshots and save.tmp retained"};
        }
        return fail(detail);
    }
    return {true, recovery.recovered, transaction,
            "quicksave slots committed; original snapshots retained"};
}

bool CanCreateGameTemp(const char16_t* source) {
    const auto path = GamePath(source);
    return path.filename() == "save.tmp" && SafeDirectory(path.parent_path()) &&
           FileKind(path) == Kind::missing;
}

Result PrepareGameTemp(const char16_t* source) {
    const auto temp = GamePath(source);
    if (temp.filename() != "save.tmp" || !SafeDirectory(temp.parent_path()))
        return {false, false, {}, "unsafe temporary save path"};
    if (FileKind(temp) == Kind::missing) return {true, false, {}, "temporary path available"};
    Fingerprint previous;
    if (!HashFile(temp, previous))
        return {false, false, {}, "occupied temp is not a readable regular file; retained"};
    // The file may be incomplete. Preserve its exact bytes rather than
    // requiring a valid game-state CRC before permitting a fresh save.
    std::string validation;
    const bool loadable = ValidateSave(temp, validation);
    for (unsigned i = 1; i <= 4096; ++i) {
        const auto backup = temp.parent_path() / ("RecoveredSave-" + std::to_string(i) +
                                                  (loadable ? ".sav" : ".tmp"));
        if (FileKind(backup) != Kind::missing) continue;
        std::string error;
        if (!CopyVerified(temp, backup, previous, {}, "retain-failed-save", error) ||
            !RemoveVerified(temp, previous, {}, "release-temp-after-backup", error))
            return {false, false, backup, error + "; original temp retained unless verified removal completed"};
        return {true, true, backup, "previous failed write retained as " + backup.string()};
    }
    return {false, false, {}, "recovery file limit reached; original temp retained"};
}

bool GameSavePathExists(const char16_t* path) {
    const auto file = GamePath(path);
    return file.extension() == ".sav" && SafeDirectory(file.parent_path()) &&
           FileKind(file) == Kind::regular;
}

Result ReplaceGameSavePath(const char16_t* source, const char16_t* target, const Fault& fault) {
    const auto temp = GamePath(source);
    const auto destination = GamePath(target);
    if (temp.filename() != "save.tmp" || destination.parent_path() != temp.parent_path() ||
        destination.extension() != ".sav" || destination.filename() == "QuickSave.sav" ||
        !SafeDirectory(temp.parent_path()) || FileKind(temp) != Kind::regular ||
        (FileKind(destination) != Kind::missing && FileKind(destination) != Kind::regular))
        return {false, false, {}, "manual save path or existing target is unsafe"};
    const auto recovery = RecoverManual(temp.parent_path());
    if (!recovery.ok) return recovery;
    std::string detail;
    if (!ValidateSave(temp, detail))
        return {false, false, {}, "manual save rejected: " + detail};
    Fingerprint next, previous;
    if (!HashFile(temp, next) || !SyncFile(temp))
        return {false, false, {}, "manual temp read/flush failed; retained"};
    fs::path backup;
    const bool occupied = FileKind(destination) == Kind::regular;
    if (occupied) {
        if (!HashFile(destination, previous)) return {false, false, {}, "old manual save unreadable"};
        for (unsigned i = 0; i < 1024; ++i) {
            auto candidate = destination;
            candidate += i == 0 ? ".bak" : ".bak." + std::to_string(i);
            if (FileKind(candidate) == Kind::missing) { backup = candidate; break; }
        }
        if (backup.empty() || !CopyVerified(destination, backup, previous, fault, "manual-backup", detail))
            return {false, false, backup, "manual backup failed; old save unchanged: " + detail};
    }
    const auto journal_root = temp.parent_path() / ".e2e4-manual-transactions";
    if ((FileKind(journal_root) != Kind::missing && !SafeDirectory(journal_root)) ||
        !Step(fault, "manual-journal-create"))
        return {false, false, backup, "manual journal unavailable; old slot unchanged"};
    std::error_code journal_error;
    fs::create_directories(journal_root, journal_error);
    if (journal_error || !SafeDirectory(journal_root))
        return {false, false, backup, "manual journal directory failed; old slot unchanged"};
    fs::path record;
    for (unsigned i = 0; i < 4096; ++i) {
        const auto candidate = journal_root / ("txn-" + std::to_string(i));
        if (FileKind(candidate) == Kind::missing) { record = candidate; break; }
    }
    if (record.empty() || !fs::create_directory(record, journal_error) || journal_error)
        return {false, false, backup, "manual journal capacity/create failed; old slot unchanged"};
    const ManualManifest manifest{destination.filename(), occupied ? backup.filename() : fs::path{}, previous, next};
    const auto manifest_text = ManualText(manifest);
    if (!WriteMarker(record / "manifest", manifest_text, fault, "manual-manifest", detail) ||
        !MarkerIs(record / "manifest", manifest_text.c_str()) ||
        !WriteMarker(record / "ready", "prepared\n", fault, "manual-ready", detail) ||
        !MarkerIs(record / "ready", "prepared\n")) {
        CleanManual(record);
        return {false, false, backup, "manual preparation failed; old slot unchanged: " + detail};
    }
    auto fail = [&](const std::string& reason) -> Result {
        const auto rollback = RecoverManual(temp.parent_path());
        return {false, rollback.ok && occupied, backup,
                reason + "; " + rollback.detail + "; new generation retained in temp"};
    };
    if (occupied && !RemoveVerified(destination, previous, fault, "manual-remove-old", detail))
        return fail(detail);
    // Use the same verified copy/flush/readback primitive as quicksave. SD
    // rename success is not sufficient: hardware reported missing destinations
    // even after a successful rename; failed installs then poisoned temp.
    if (!CopyVerified(temp, destination, next, fault, "manual-publish", detail)) {
        return fail(detail);
    }
    if (!WriteMarker(record / "committed", "complete\n", fault, "manual-commit", detail) ||
        !MarkerIs(record / "committed", "complete\n")) {
        // A failed marker write has not acknowledged the new generation.
        if (FileKind(record / "committed") == Kind::regular) {
            Fingerprint marker;
            if (!HashFile(record / "committed", marker) ||
                !RemoveVerified(record / "committed", marker, {}, "manual-clear-failed-commit", detail))
                return {false, false, backup, "manual commit marker unsafe; temp and backup retained"};
        }
        return fail(detail);
    }
    CleanManual(record);
    if (!RemoveVerified(temp, next, {}, "manual-release-temp", detail))
        return {true, false, backup, "manual save verified; temp cleanup deferred: " + detail};
    return {true, false, backup, "manual save installed and readback verified; previous generation retained in backup"};
}

Result RotateGamePaths(const char16_t* source, const char16_t* quick_slot_one,
                       std::int32_t extra_slots) {
    const auto temp = GamePath(source);
    const auto quick = GamePath(quick_slot_one);
    if (temp.filename() != "save.tmp" || quick != SlotPath(temp.parent_path(), 0) ||
        extra_slots < 0 || extra_slots > 31)
        return {false, false, {}, "invalid game quicksave path or slot count"};
    std::vector<fs::path> slots;
    for (std::int32_t i = 0; i <= extra_slots; ++i)
        slots.push_back(SlotPath(temp.parent_path(), static_cast<std::size_t>(i)));
    return Rotate(temp, slots);
}

Result RecoverGamePaths(const char16_t* quick_slot_one) {
    const auto quick = GamePath(quick_slot_one);
    const auto directory = quick.parent_path();
    if (quick != SlotPath(directory, 0) || directory.empty())
        return {false, false, {}, "invalid quicksave directory"};
    if (FileKind(directory) == Kind::missing)
        return {true, false, {}, "no save directory or pending rotation"};
    return Recover(directory);
}

} // namespace srhd_awa::platform::e2e4_quicksave
