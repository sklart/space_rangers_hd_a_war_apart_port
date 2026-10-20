#include "e2e4_quicksave_transaction.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace srhd_awa::platform::e2e4_quicksave;
std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
int main(int argc, char** argv) {
    if (argc != 4 || fs::exists(argv[3])) return 2;
    const fs::path old_source(argv[1]), new_source(argv[2]), root(argv[3]);
    std::string detail;
    if (!ValidateSave(old_source, detail) || !ValidateSave(new_source, detail)) return 3;
    const auto old_bytes = Read(old_source), new_bytes = Read(new_source);
    if (old_bytes.empty() || old_bytes == new_bytes) return 4;
    unsigned failures{}, interruptions{}, fresh_interruptions{};
    for (const auto& name : {fs::path("Occupied.sav"), fs::path(u"Занятый Ёж.sav")}) {
        const auto base = root / (name == fs::path("Occupied.sav") ? "latin" : "unicode");
        auto prepare = [&](const fs::path& dir) {
            fs::create_directories(dir);
            fs::copy_file(old_source, dir / name);
            fs::copy_file(new_source, dir / "save.tmp");
        };
        const auto normal = base / "normal"; prepare(normal);
        std::vector<std::string> operations;
        const auto temp = (normal / "save.tmp").u16string(), target = (normal / name).u16string();
        auto result = ReplaceGameSavePath(temp.c_str(), target.c_str(), [&](const char* operation) {
            operations.emplace_back(operation); return true;
        });
        if (!result.ok || Read(normal / name) != new_bytes) return 5;
        for (unsigned at = 1; at <= operations.size(); ++at) {
            const auto dir = base / ("fail-" + std::to_string(at)); prepare(dir);
            unsigned count{};
            result = ReplaceGameSavePath((dir / "save.tmp").u16string().c_str(), (dir / name).u16string().c_str(),
                [&](const char*) { return ++count != at; });
            if (result.ok || Read(dir / name) != old_bytes || Read(dir / "save.tmp") != new_bytes) {
                std::cerr << "FAIL return-fault at=" << operations[at - 1] << '\n'; return 6;
            }
            if (!Recover(dir).ok || !Recover(dir).ok ||
                !ReplaceGameSavePath((dir / "save.tmp").u16string().c_str(), (dir / name).u16string().c_str()).ok ||
                Read(dir / name) != new_bytes) return 11;
            ++failures;
        }
        for (unsigned at = 1; at <= operations.size(); ++at) {
            const auto dir = base / ("stop-" + std::to_string(at)); prepare(dir);
            const auto child = fork();
            if (child < 0) return 7;
            if (!child) {
                unsigned count{};
                ReplaceGameSavePath((dir / "save.tmp").u16string().c_str(), (dir / name).u16string().c_str(),
                    [&](const char*) { if (++count == at) std::_Exit(99); return true; });
                std::_Exit(98);
            }
            int status{};
            if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 99) return 8;
            const auto recovery = Recover(dir);
            if (!recovery.ok || Read(dir / name) != old_bytes || Read(dir / "save.tmp") != new_bytes) {
                std::cerr << "FAIL process-stop at=" << operations[at - 1]
                          << " primary_exists=" << fs::exists(dir / name)
                          << " backup_exists=" << fs::exists(fs::path((dir / name).string() + ".bak"))
                          << " recovery=" << recovery.detail << '\n';
                return 9;
            }
            if (!Recover(dir).ok ||
                !ReplaceGameSavePath((dir / "save.tmp").u16string().c_str(), (dir / name).u16string().c_str()).ok ||
                Read(dir / name) != new_bytes) return 12;
            ++interruptions;
        }
    }
    // A new slot has no previous generation: recovery must withdraw an
    // uncommitted destination, retain the temp and permit a normal retry.
    for (const auto* boundary : {"manual-publish", "copy-before-flush", "manual-commit"}) {
        const auto dir = root / (std::string("fresh-") + boundary);
        fs::create_directories(dir);
        fs::copy_file(new_source, dir / "save.tmp");
        const auto target = dir / fs::path(u"Новый Ёж.sav");
        const auto child = fork();
        if (child < 0) return 13;
        if (!child) {
            bool publishing = false;
            ReplaceGameSavePath((dir / "save.tmp").u16string().c_str(), target.u16string().c_str(),
                [&](const char* operation) {
                    if (std::string(operation) == "manual-publish") publishing = true;
                    if (publishing && std::string(operation) == boundary) std::_Exit(99);
                    return true;
                });
            std::_Exit(98);
        }
        int status{};
        if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 99) return 14;
        if (!Recover(dir).ok || fs::exists(target) || Read(dir / "save.tmp") != new_bytes ||
            !Recover(dir).ok ||
            !ReplaceGameSavePath((dir / "save.tmp").u16string().c_str(), target.u16string().c_str()).ok ||
            Read(target) != new_bytes) return 15;
        ++fresh_interruptions;
    }
    if (Read(old_source) != old_bytes || Read(new_source) != new_bytes) return 10;
    std::cout << "PASS real RSG64 files; return_faults=" << failures
              << " process_interruptions=" << interruptions << " retries=" << failures + interruptions
              << " fresh_interruptions=" << fresh_interruptions
              << " latin_and_unicode=1 originals_unchanged=1\n";
}
