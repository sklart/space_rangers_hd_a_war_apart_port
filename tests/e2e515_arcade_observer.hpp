#pragma once
// Diagnostic translation units only. Never included in the shipping game.
#include "types/ab_MainForm.hpp"
#include "types/ab_Hit.hpp"
#include "types/GI_Panel.hpp"
#include "units/ab_Global.hpp"
#include "units/ab_Object.hpp"
#include "units/ab_Ship.hpp"
#include "units/System.hpp"
#include "renderer_platform.hpp"
#include "types/ab_WorldLine.hpp"
#include "units/ab_WorldLine.hpp"
#include "types/GI_PolyLine.hpp"
#include "units/GR_Main.hpp"
#include "e2e515_arcade_clock.hpp"
#include <array>
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
struct Hotspot { std::uint64_t calls{}, samples{}, cpu{}, maximum{}; };
inline thread_local bool measuring{};
inline thread_local std::array<Hotspot, 8> hotspots{};
inline constexpr const char* names[] = {"unused", "bearing-distance", "object-distance",
    "object-advance", "ship-ai-advance", "decide-actions", "avoid-obstacle", "matrix-projection"};
struct Scope {
    unsigned id;
    bool enabled{measuring};
    std::uint64_t begin{};
    explicit Scope(unsigned value) : id(value) {
        if (enabled) enabled = (++hotspots[id].calls % 64) == 1;
        if (enabled) begin = Cpu();
    }
    ~Scope() {
        if (!enabled) return;
        const auto used = Cpu() - begin;
        auto& target = hotspots[id]; ++target.samples; target.cpu += used;
        if (used > target.maximum) target.maximum = used;
    }
};
inline void Row(const char* event, std::uint64_t duration = 0, std::uint64_t cpu = 0) {
    if (!output) return;
    std::fprintf(output, "%s,%llu,%d,%llu,%llu,%u\n", event,
        static_cast<unsigned long long>(Now() - start), ab_Global::ArcadeTickCount,
        static_cast<unsigned long long>(duration), static_cast<unsigned long long>(cpu), presents);
    std::fflush(output);
}
inline void BeginReplayClock(ab_MainForm::TfAB* form) {
    if (!Option("E2E515_ARCADE_REPLAY", 0)) return;
    // Rebase already-created UI timers, preserving every relative deadline.
    const std::uint32_t base = 100000;
    const auto previous = form->TimerTick;
    for (auto* timer = form->FirstTimer; timer; timer = timer->Next)
        timer->DueTick = timer->DueTick <= previous ? base : base + (timer->DueTick - previous);
    form->TimerTick = base;
    e2e515_arcade_clock::milliseconds = base;
    e2e515_arcade_clock::enabled = true;
    GR_Main::LastPresentationTick = base;
}
inline void Replay(GI_MessageLoop::TMessageLoopGI* loop) {
    if (!e2e515_arcade_clock::enabled || loop != active) return;
    Row("replay-begin");
    for (unsigned elapsed = 0; elapsed < 60000 && loop->ExitCode == 0; ++elapsed) {
        ++e2e515_arcade_clock::milliseconds;
        loop->AdvanceTimerTick(); // Original timer queue and original callbacks.
        if ((elapsed + 1) % 20 == 0) loop->DrawFrame();
    }
    Row("replay-incomplete"); std::fflush(nullptr); std::_Exit(8);
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
    hash = UINT64_C(14695981039346656037);
    unsigned lines{};
    for (auto* line = ab_WorldLine::FirstWorldLine; line; line = line->Next) {
        ++lines; add(line->Kind);
        const bool visible = line->Segment != nullptr; add(visible);
        if (!visible) continue;
        add(line->Segment->First.X); add(line->Segment->First.Y);
        add(line->Segment->Last.X); add(line->Segment->Last.Y);
        add(line->Segment->PixelCount); add(line->Segment->Color); add(line->Segment->EndColor);
    }
    std::fprintf(stderr, "ARCADE_GEOMETRY tick=%d lines=%u hash=%016llx\n",
        ab_Global::ArcadeTickCount, lines, static_cast<unsigned long long>(hash));
    std::fflush(stderr);
}
inline void Finish() {
    if (e2e515_arcade_clock::enabled) Row("replay-logical-ms", e2e515_arcade_clock::milliseconds - 100000);
    Checkpoint(); Row("complete", Now() - start, Cpu() - start_cpu);
    if (auto* file = std::fopen("arcade-hotspots.csv", "w")) {
        for (unsigned i = 1; i < hotspots.size(); ++i)
            std::fprintf(file, "%s,%llu,%llu,%llu,%llu\n", names[i],
                static_cast<unsigned long long>(hotspots[i].calls),
                static_cast<unsigned long long>(hotspots[i].samples),
                static_cast<unsigned long long>(hotspots[i].cpu),
                static_cast<unsigned long long>(hotspots[i].maximum));
        std::fclose(file);
    }
    std::fclose(output); output = nullptr; std::fflush(nullptr); std::_Exit(0);
}
struct Tick {
    ab_MainForm::TfAB* self;
    std::uint64_t begin{Now()}, cpu{Cpu()};
    explicit Tick(ab_MainForm::TfAB* form) : self(form) {
        if (!output) {
            output = std::fopen("arcade-events.csv", "w");
            if (!output) std::_Exit(3);
            active = form; start = begin; start_cpu = cpu; measuring = true; Row("begin");
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
        Row("rng", System::RandSeed, self->RandomSeed);
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
    Row("image-hash", srhd_awa::platform::renderer_platform::LastPresentationHash());
    if (result && !result_presented) { result_presented = true; Row("result-presented"); }
    if (result_presented && explosion_complete) Finish();
}
inline void ExplosionComplete(ab_Hit::TabHit* hit) {
    if (hit == ab_Ship::PlayerArcadeShip) { explosion_complete = true; Row("explosion-complete"); }
}
}
