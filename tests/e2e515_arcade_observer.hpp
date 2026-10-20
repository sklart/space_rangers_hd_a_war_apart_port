#pragma once
// Diagnostic translation units only. Never included in the shipping game.
#include "types/ab_MainForm.hpp"
#include "types/ab_Hit.hpp"
#include "types/GI_Panel.hpp"
#include "units/ab_Global.hpp"
#include "units/ab_Object.hpp"
#include "units/ab_Ship.hpp"
#include "units/System.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

namespace e2e515_arcade_observer {
inline std::uint64_t Now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::uint64_t Cpu() {
    timespec t{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return std::uint64_t(t.tv_sec) * 1000000000ull + t.tv_nsec;
}
inline int Option(const char* key, int fallback) {
    const auto* text = std::getenv(key); return text ? std::atoi(text) : fallback;
}
inline FILE* output{};
inline ab_MainForm::TfAB* active{};
inline bool lethal{}, result{}, result_presented{}, explosion_complete{};
inline unsigned presents{};
inline std::uint64_t start{}, start_cpu{};
inline void Row(const char* event, std::uint64_t duration = 0, std::uint64_t cpu = 0) {
    if (!output) return;
    std::fprintf(output, "%s,%llu,%d,%llu,%llu,%u\n", event,
        static_cast<unsigned long long>(Now() - start), ab_Global::ArcadeTickCount,
        static_cast<unsigned long long>(duration), static_cast<unsigned long long>(cpu), presents);
    std::fflush(output);
}
inline void Checkpoint() {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    auto add = [&]<class T>(const T& value) {
        for (unsigned i = 0; i < sizeof(T); ++i) {
            hash ^= reinterpret_cast<const unsigned char*>(&value)[i];
            hash *= UINT64_C(1099511628211);
        }
    };
    add(active->RandomSeed); add(System::RandSeed); add(ab_Global::ArcadeTickCount);
    unsigned objects{};
    for (auto* obj = ab_Object::FirstArcadeObject; obj; obj = obj->Next) {
        ++objects; add(obj->InitialRandomSeed); add(obj->RandomState);
        add(obj->State.LongitudeDegrees); add(obj->State.PolarAngleDegrees); add(obj->State.BearingDegrees);
        add(obj->Velocity.X); add(obj->Velocity.Y); add(obj->DeletionPending); add(obj->DistanceTravelled);
        if (auto* hit = pas::class_cast_if<ab_Hit::TabHit*>(obj)) { add(hit->Health); add(hit->MaxHealth); }
    }
    std::fprintf(stderr, "ARCADE_CHECKPOINT tick=%d objects=%u hash=%016llx\n",
        ab_Global::ArcadeTickCount, objects, static_cast<unsigned long long>(hash));
    std::fflush(stderr);
}
inline void Finish() {
    Checkpoint(); Row("complete", Now() - start, Cpu() - start_cpu);
    std::fclose(output); output = nullptr; std::fflush(nullptr); std::_Exit(0);
}
struct Tick {
    ab_MainForm::TfAB* self;
    std::uint64_t begin{Now()}, cpu{Cpu()};
    explicit Tick(ab_MainForm::TfAB* form) : self(form) {
        if (!output) {
            output = std::fopen("arcade-events.csv", "w");
            if (!output) std::_Exit(3);
            active = form; start = begin; start_cpu = cpu; Row("begin");
        }
        if (ab_Global::ArcadeTickCount > 0) {
            const int death_tick = Option("E2E515_ARCADE_DEATH_TICK", 0);
            if (death_tick && !lethal && ab_Global::ArcadeTickCount >= death_tick && ab_Ship::PlayerArcadeShip) {
                lethal = true; Row("lethal-hit");
                // Invoke the original damage/death/effect path, never fabricate a result panel.
                ab_Ship::PlayerArcadeShip->ApplyDamage(10000000, nullptr, false);
                Row("death");
            }
            const int slow = Option("E2E515_ARCADE_SLOW_MS", 0);
            if (slow) std::this_thread::sleep_for(std::chrono::milliseconds(slow));
        }
    }
    ~Tick() {
        if (std::uncaught_exceptions()) { Row("exception"); return; }
        Row("tick", Now() - begin, Cpu() - cpu);
        if (lethal && self->DefeatPanel->Active && !result) { result = true; Row("result"); }
        const int tick = ab_Global::ArcadeTickCount;
        if (tick == 500 || tick == 1000) Checkpoint();
        if (!Option("E2E515_ARCADE_DEATH_TICK", 0) && tick >= Option("E2E515_ARCADE_TICKS", 500)) Finish();
    }
};
struct Draw {
    std::uint64_t begin{Now()}, cpu{Cpu()};
    ~Draw() { Row("draw", Now() - begin, Cpu() - cpu); }
};
inline void Present() {
    if (!output) return;
    ++presents; Row("present");
    if (result && !result_presented) { result_presented = true; Row("result-presented"); }
    if (result_presented && explosion_complete) Finish();
}
inline void ExplosionComplete(ab_Hit::TabHit* hit) {
    if (hit == ab_Ship::PlayerArcadeShip) { explosion_complete = true; Row("explosion-complete"); }
}
}
