#include "continuous_time.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
    // The GI timer advances one millisecond at a time and invokes TimerTakt
    // every 20 ms. Fractional 16.7 ms frames are represented by an exact
    // repeating integer sequence, as with timeGetTime on the real loop.
    for (const auto frame_tenths : {167u, 330u, 1000u, 1660u, 2500u}) {
        std::uint32_t elapsed{}, advanced{}, old_advanced{}, old_ticks{}, ticks{};
        for (unsigned frame = 0; frame < 600; ++frame) {
            const auto now = (frame + 1) * frame_tenths / 10;
            const auto draw = now - elapsed;
            elapsed = now;
            advanced += srhd_awa::platform::ContinuousTimerMillis(draw, 0, true);
            old_advanced += (draw > 40 ? 40 : draw);
            ticks = advanced / 20;
            old_ticks = old_advanced / 20;
            assert(ticks == elapsed / 20);
            assert(advanced - ticks * 20 < 20);
        }
        std::printf("frame_tenths=%u wall_ms=%u old_ticks=%u ticks=%u backlog_ms=%u\n",
                    frame_tenths, elapsed, old_ticks, ticks, advanced - ticks * 20);
    }
    // Expensive logical updates still count as elapsed time next frame.
    assert(srhd_awa::platform::ContinuousTimerMillis(166, 63, true) == 229);
    assert(srhd_awa::platform::ContinuousTimerMillis(250, 300, true) == 550);
    // Film playback keeps its previous limit independently of the arcade.
    assert(srhd_awa::platform::ContinuousTimerMillis(250, 300, false) == 200);
    // While paused the timer keeps ticking but the game's existing pause
    // check suppresses simulation callbacks. There is no deferred pause debt.
    unsigned timer{}, logical_ticks{};
    auto run = [&](unsigned ms, bool paused) {
        for (unsigned i = 0; i < srhd_awa::platform::ContinuousTimerMillis(ms, 0, true); ++i)
            if (++timer % 20 == 0 && !paused) ++logical_ticks;
    };
    run(1000, false); assert(logical_ticks == 50);
    run(5000, true); assert(logical_ticks == 50);
    run(1000, false); assert(logical_ticks == 100);
    std::puts("PASS timer delivery contract; gameplay and device performance are separate checks");
}
