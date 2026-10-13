#include "e2e4_quicksave_transaction.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdio.h>
#include <system_error>
#include <vector>

#include <zlib.h>

#ifdef _WIN32
#include <io.h>
#else
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
    const auto name = path.string();
    FILE* file = std::fopen(name.c_str(), "rb+");
    if (!file) return false;
    const bool ok = _commit(_fileno(file)) == 0;
#else
    FILE* file = std::fopen(path.c_str(), "rb+");
    if (!file) return false;
    const bool ok = fsync(fileno(file)) == 0;
#endif
    return std::fclose(file) == 0 && ok;
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
    if (!fs::copy_file(from, to, fs::copy_options::none, ec) || ec ||
        FileKind(to) != Kind::regular) {
        error = std::string(operation) + ": copy or flush failed: " + ec.message() +
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
    FILE* file = std::fopen(path.string().c_str(), "wb");
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
    std::string utf8;
    for (; *wide; ++wide) {
        auto code = static_cast<std::uint32_t>(*wide);
        if (code == '\\') code = '/';
        if (code < 0x80) utf8.push_back(static_cast<char>(code));
        else if (code < 0x800) {
            utf8.push_back(static_cast<char>(0xc0 | (code >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code >= 0xd800 && code <= 0xdbff && wide[1] >= 0xdc00 && wide[1] <= 0xdfff) {
            code = 0x10000 + ((code - 0xd800) << 10) + (static_cast<std::uint32_t>(*++wide) - 0xdc00);
            utf8.push_back(static_cast<char>(0xf0 | (code >> 18)));
            utf8.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            utf8.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            utf8.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code >= 0xd800 && code <= 0xdfff) return {};
        else {
            utf8.push_back(static_cast<char>(0xe0 | (code >> 12)));
            utf8.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            utf8.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return fs::path(utf8).lexically_normal();
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
    const auto root = save_directory / ".e2e4-quicksave-backups";
    if (FileKind(root) == Kind::missing) return {true, false, {}, "no transactions"};
    if (FileKind(root) != Kind::directory || !SafeDirectory(root))
        return {false, false, root, "unsafe backup root"};
    bool recovered = false;
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
