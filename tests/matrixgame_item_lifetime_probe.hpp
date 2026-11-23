// Test-only event registry: it records object lifetimes without dereferencing
// opaque script values and never participates in game/save logic.
#pragma once
#include <cstdint>
#include <cstdio>
#include <array>
#include <mutex>
#include <unordered_map>

extern "C" __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(
    unsigned long, unsigned long, void**, unsigned long*);
extern "C" char __ImageBase;
namespace matrixgame_item_lifetime_probe {
struct Record { std::uint32_t id{}; bool live{}; unsigned long long generation{}; std::array<void*, 20> stack{}, create_stack{}; unsigned short stack_count{}, create_count{}; };
struct Assignment { std::uintptr_t value{}; std::uint32_t id{}; bool observed{}, live{}, assigned_object_destroyed{}; unsigned long long generation{}; std::array<void*, 20> stack{}, assigned_destroy_stack{}, create_stack{}; unsigned short stack_count{}, assigned_destroy_count{}, create_count{}; };
inline std::mutex& Lock() { static auto* value = new std::mutex; return *value; }
inline std::unordered_map<std::uintptr_t, Record>& Records() {
    static auto* value = new std::unordered_map<std::uintptr_t, Record>; return *value;
}
inline std::unordered_map<const void*, Assignment>& Assignments() {
    static auto* value = new std::unordered_map<const void*, Assignment>; return *value;
}
inline void Assigned(const void* cell, std::uintptr_t value) {
    std::lock_guard guard(Lock()); auto& assignment = Assignments()[cell]; assignment = {};
    assignment.value = value; const auto found = Records().find(value);
    if (found != Records().end()) { assignment.observed = true; assignment.id = found->second.id;
        assignment.live = found->second.live; assignment.generation = found->second.generation;
        assignment.create_stack = found->second.create_stack; assignment.create_count = found->second.create_count; }
    assignment.stack_count = RtlCaptureStackBackTrace(0, static_cast<unsigned long>(assignment.stack.size()), assignment.stack.data(), nullptr);
}
inline void ReportAssignment(const void* cell, std::uintptr_t value) {
    std::lock_guard guard(Lock()); const auto found = Assignments().find(cell);
    if (found == Assignments().end()) { std::puts("SAVE_PREFLIGHT quest_item_assignment=UNOBSERVED"); return; }
    const auto& a = found->second;
    std::printf("SAVE_PREFLIGHT quest_item_assignment matches=%d observed=%d live=%d id=%u generation=%llu\n", a.value==value, a.observed, a.live, a.id, a.generation);
    const auto base = reinterpret_cast<std::uintptr_t>(&__ImageBase);
    std::printf("SAVE_PREFLIGHT quest_item_created address=%llx id=%u generation=%llu image=%p:",
                static_cast<unsigned long long>(a.value), a.id, a.generation, static_cast<void*>(&__ImageBase));
    for (unsigned i=0;i<a.create_count;++i) std::printf(" %llx",static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(a.create_stack[i])-base));
    std::puts("");
    std::printf("SAVE_PREFLIGHT quest_item_assign_stack image=%p:", static_cast<void*>(&__ImageBase));
    for (unsigned i=0;i<a.stack_count;++i) std::printf(" %llx",static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(a.stack[i])-base));
    std::puts("");
    std::printf("SAVE_PREFLIGHT quest_item_assigned_object_destroyed=%d\n", a.assigned_object_destroyed);
    if (a.assigned_object_destroyed) {
        std::printf("SAVE_PREFLIGHT quest_item_assigned_destroy_stack image=%p:", static_cast<void*>(&__ImageBase));
        for (unsigned i=0;i<a.assigned_destroy_count;++i) std::printf(" %llx",static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(a.assigned_destroy_stack[i])-base));
        std::puts("");
    }
}
inline void Created(const void* object, std::uint32_t id) {
    std::lock_guard guard(Lock()); auto& entry = Records()[reinterpret_cast<std::uintptr_t>(object)];
    ++entry.generation; entry.id = id; entry.live = true;
    entry.create_count = RtlCaptureStackBackTrace(0, static_cast<unsigned long>(entry.create_stack.size()), entry.create_stack.data(), nullptr);
}
inline void Destroyed(const void* object, std::uint32_t id) {
    std::lock_guard guard(Lock()); auto& entry = Records()[reinterpret_cast<std::uintptr_t>(object)];
    entry.id = id; entry.live = false;
    entry.stack_count = RtlCaptureStackBackTrace(0, static_cast<unsigned long>(entry.stack.size()), entry.stack.data(), nullptr);
    for (auto& [cell, assignment] : Assignments()) {
        if (assignment.value == reinterpret_cast<std::uintptr_t>(object) && assignment.observed &&
            assignment.generation == entry.generation && assignment.id == id) {
            assignment.assigned_object_destroyed = true;
            assignment.assigned_destroy_count = entry.stack_count;
            assignment.assigned_destroy_stack = entry.stack;
        }
    }
}
inline void Report(std::uintptr_t value) {
    std::lock_guard guard(Lock()); const auto found = Records().find(value);
    if (found == Records().end()) { std::puts("SAVE_PREFLIGHT quest_item_lifetime=UNOBSERVED"); return; }
    std::printf("SAVE_PREFLIGHT quest_item_lifetime=%s id=%u generation=%llu\n",
                found->second.live ? "LIVE" : "DESTROYED", found->second.id, found->second.generation);
    const auto base = reinterpret_cast<std::uintptr_t>(&__ImageBase);
    std::printf("SAVE_PREFLIGHT quest_item_destroy_stack image=%p:", static_cast<void*>(&__ImageBase));
    for (unsigned i = 0; i < found->second.stack_count; ++i)
        std::printf(" %llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(found->second.stack[i]) - base));
    std::puts("");
}
}
