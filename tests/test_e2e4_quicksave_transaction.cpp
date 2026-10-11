#include "e2e4_quicksave_transaction.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

namespace fs = std::filesystem;
using namespace srhd_awa::platform::e2e4_quicksave;

static void U32(std::vector<unsigned char>& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (i * 8)));
}
static void U16Text(std::vector<unsigned char>& out, const std::string& value) {
    for (const char byte : value) { out.push_back(static_cast<unsigned char>(byte)); out.push_back(0); }
    out.push_back(0); out.push_back(0);
}
static std::vector<unsigned char> ValidSave() {
    std::vector<unsigned char> out;
    for (const auto& text : {"RSG", "1.0", "test", "1", "100", "pilot", "race", "EZ"})
        U16Text(out, text);
    U32(out, 0); U32(out, 0);
    const std::string state = "synthetic checked galaxy state for quicksave rotation";
    std::vector<unsigned char> compressed(compressBound(state.size()));
    uLongf size = compressed.size();
    assert(compress2(compressed.data(), &size,
                     reinterpret_cast<const Bytef*>(state.data()), state.size(), 1) == Z_OK);
    compressed.resize(size);
    U32(out, static_cast<std::uint32_t>(crc32(0,
        reinterpret_cast<const Bytef*>(compressed.data()), compressed.size())));
    constexpr std::int32_t seed = 1234567;
    U32(out, seed);
    U32(out, static_cast<std::uint32_t>(compressed.size()));
    std::int32_t rng = seed;
    for (auto byte : compressed) {
        const auto next = 16807 * (rng % 127773) - 2836 * (rng / 127773);
        rng = next <= 0 ? next + 0x7fffffff : next;
        out.push_back(byte ^ static_cast<unsigned char>(rng - 1));
    }
    return out;
}
static void Write(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    file.close(); assert(file.good());
}
static void Write(const fs::path& path, const std::string& text) {
    Write(path, std::vector<unsigned char>(text.begin(), text.end()));
}
static std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static std::vector<fs::path> Slots(const fs::path& dir) {
    return {dir / "QuickSave.sav", dir / "QuickSave2.sav", dir / "QuickSave3.sav"};
}
static void Populate(const fs::path& dir) {
    fs::create_directories(dir);
    auto slots = Slots(dir);
    Write(slots[0], "old-1"); Write(slots[1], "old-2"); Write(slots[2], "old-3");
    Write(dir / "save.tmp", ValidSave());
    Write(dir / "QuickSave.sav.bak", "pre-existing-user-backup");
}
static void OriginalIntact(const fs::path& dir) {
    auto slots = Slots(dir);
    assert(Read(slots[0]) == "old-1");
    assert(Read(slots[1]) == "old-2");
    assert(Read(slots[2]) == "old-3");
    assert(Read(dir / "QuickSave.sav.bak") == "pre-existing-user-backup");
    assert(fs::is_regular_file(dir / "save.tmp"));
}

int main(int argc, char** argv) {
    assert(argc == 2);
    const fs::path base(argv[1]);
    fs::create_directories(base);
    const auto root = base / ("run-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(fs::create_directory(root));
    const auto valid = ValidSave();
    {
        const auto dir = root / "success"; Populate(dir);
        std::string detail;
        assert(ValidateSave(dir / "save.tmp", detail));
        auto result = Rotate(dir / "save.tmp", Slots(dir));
        if (!result.ok) std::fprintf(stderr, "rotate failure: %s\n", result.detail.c_str());
        assert(result.ok);
        assert(Read(Slots(dir)[1]) == "old-1");
        assert(Read(Slots(dir)[2]) == "old-2");
        assert(Read(dir / "QuickSave.sav.bak") == "pre-existing-user-backup");
        assert(Read(result.backup_directory / "slot3.orig") == "old-3");
        assert(Read(result.backup_directory / "slot1.orig") == "old-1");
        assert(Read(Slots(dir)[0]) == std::string(valid.begin(), valid.end()));
        assert(fs::is_regular_file(dir / "save.tmp"));
        assert(Recover(dir).ok);
        // All backup slots are reserved. A new save must fail before touching slots.
        for (unsigned i = 2; i <= 16; ++i) {
            char name[32]; std::snprintf(name, sizeof(name), "txn-%02u", i);
            fs::create_directory(dir / ".e2e4-quicksave-backups" / name);
            fs::copy_file(result.backup_directory / "manifest",
                          dir / ".e2e4-quicksave-backups" / name / "manifest");
            Write(dir / ".e2e4-quicksave-backups" / name / "committed", "complete\n");
        }
        result = Rotate(dir / "save.tmp", Slots(dir));
        assert(!result.ok && result.detail.find("limit") != std::string::npos);
        assert(Read(Slots(dir)[1]) == "old-1");
    }
    {
        const auto dir = root / "invalid"; Populate(dir);
        auto bytes = valid;
        bytes.back() ^= 1;
        Write(dir / "save.tmp", bytes);
        assert(!Rotate(dir / "save.tmp", Slots(dir)).ok);
        OriginalIntact(dir);
        Write(dir / "save.tmp", std::vector<unsigned char>(valid.begin(), valid.end() - 1));
        assert(!Rotate(dir / "save.tmp", Slots(dir)).ok);
        OriginalIntact(dir);
    }
    {
        const auto dir = root / "sparse";
        fs::create_directories(dir);
        Write(Slots(dir)[0], "single-old-slot");
        Write(dir / "save.tmp", valid);
        const auto result = Rotate(dir / "save.tmp", Slots(dir));
        assert(result.ok);
        assert(Read(Slots(dir)[1]) == "single-old-slot");
        assert(!fs::exists(Slots(dir)[2]));
        assert(Read(result.backup_directory / "slot1.orig") == "single-old-slot");
        assert(!fs::exists(result.backup_directory / "slot2.orig"));
    }
    // Count the live operation hooks so a new stage cannot silently escape
    // the exhaustive failure and crash loops below.
    int operation_count = 0;
    {
        const auto dir = root / "count-operations"; Populate(dir);
        assert(Rotate(dir / "save.tmp", Slots(dir), [&](const char*) {
            ++operation_count; return true;
        }).ok);
    }
    assert(operation_count > 20);
    for (const char* failed_read : {"read-new-save", "read-original-slot"}) {
        const auto dir = root / failed_read; Populate(dir);
        const auto result = Rotate(dir / "save.tmp", Slots(dir), [failed_read](const char* step) {
            return std::string(step) != failed_read;
        });
        assert(!result.ok);
        OriginalIntact(dir);
    }
    // Fail every read and mutating stage. Successful rollback retains backup.
    for (int fail_at = 1; fail_at <= operation_count; ++fail_at) {
        const auto dir = root / ("fault-" + std::to_string(fail_at)); Populate(dir);
        int call = 0;
        const Fault fail = [&](const char*) { return ++call != fail_at; };
        const auto result = Rotate(dir / "save.tmp", Slots(dir), fail);
        assert(!result.ok);
        OriginalIntact(dir);
        const auto recovery = Recover(dir);
        assert(recovery.ok || recovery.detail.find("manifest") != std::string::npos);
    }
    // A process can stop without running the in-process rollback. Recovery
    // must reconstruct every original slot from the on-disk manifest.
    for (int stop_at = 1; stop_at <= operation_count; ++stop_at) {
        const auto dir = root / ("crash-" + std::to_string(stop_at)); Populate(dir);
        int call = 0;
        try {
            Rotate(dir / "save.tmp", Slots(dir), [&](const char*) {
                if (++call == stop_at) throw std::runtime_error("simulated power interruption");
                return true;
            });
            assert(false);
        } catch (const std::runtime_error&) {}
        auto recovered = Recover(dir);
        if (fs::exists(dir / ".e2e4-quicksave-backups" / "txn-01" / "manifest"))
            assert(recovered.ok && recovered.recovered);
        OriginalIntact(dir);
    }
    {
        const auto dir = root / "late-collision"; Populate(dir);
        bool inserted = false;
        const auto result = Rotate(dir / "save.tmp", Slots(dir), [&](const char* step) {
            if (!inserted && std::string(step) == "publish-copy") {
                Write(Slots(dir)[0], "late-user-file");
                inserted = true;
            }
            return true;
        });
        assert(inserted && !result.ok);
        OriginalIntact(dir);
        assert(Read(result.backup_directory / "slot1.interrupted") == "late-user-file");
    }
    {
        // A persistent write failure may also prevent automatic rollback.
        // Recovery must retain snapshots and succeed once space is available.
        const auto dir = root / "no-space"; Populate(dir);
        const auto result = Rotate(dir / "save.tmp", Slots(dir), [](const char* step) {
            const std::string operation(step);
            return operation != "publish-copy" && operation != "restore-copy";
        });
        assert(!result.ok && result.detail.find("rollback FAILED") != std::string::npos);
        assert(Read(result.backup_directory / "slot1.orig") == "old-1");
        assert(fs::is_regular_file(dir / "save.tmp"));
        assert(Recover(dir).ok);
        OriginalIntact(dir);
    }
    {
        const auto dir = root / "symlink"; Populate(dir);
        std::error_code ec;
        fs::remove(Slots(dir)[1]);
        fs::create_symlink(Slots(dir)[0], Slots(dir)[1], ec);
        if (!ec) {
            assert(!Rotate(dir / "save.tmp", Slots(dir)).ok);
            assert(Read(Slots(dir)[0]) == "old-1");
        }
    }
    {
        const auto dir = root / "recovery"; Populate(dir);
        auto result = Rotate(dir / "save.tmp", Slots(dir)); assert(result.ok);
        fs::remove(result.backup_directory / "committed");
        auto failed = Recover(dir, [](const char* step) {
            return std::string(step) != "quarantine-remove";
        });
        assert(!failed.ok);
        assert(Read(result.backup_directory / "slot1.orig") == "old-1");
        assert(fs::is_regular_file(dir / "save.tmp"));
        auto recovered = Recover(dir); assert(recovered.ok && recovered.recovered);
        OriginalIntact(dir);
        assert(Read(result.backup_directory / "slot1.interrupted") ==
               std::string(valid.begin(), valid.end()));
    }
    {
        const auto dir = root / "partial-restore"; Populate(dir);
        const auto transaction = Rotate(dir / "save.tmp", Slots(dir));
        assert(transaction.ok);
        fs::remove(transaction.backup_directory / "committed");
        try {
            Recover(dir, [&](const char* step) {
                if (std::string(step) == "restore-copy") {
                    Write(Slots(dir)[0], "partial-restore-write");
                    throw std::runtime_error("interrupted restoration write");
                }
                return true;
            });
            assert(false);
        } catch (const std::runtime_error&) {}
        const auto recovered = Recover(dir);
        assert(recovered.ok && recovered.recovered);
        OriginalIntact(dir);
        assert(Read(transaction.backup_directory / "slot1.interrupted.1") ==
               "partial-restore-write");
    }
}
