// SPDX-License-Identifier: GPL-2.0-or-later
// Compile the actual AbortNativeFrame prefix through its queue-health guard.
// This observes callback ownership, not libnx/GPU recovery or physical readback.
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <span>
#include "../port/matrixgame/native-engine/native_screenshot.hpp"
using DeviceResult=std::int32_t;
namespace presentation {constexpr DeviceResult DeviceLost=-1;}
struct State {
    using Callback=std::function<void(std::span<const std::byte>,unsigned,unsigned)>;
    bool queue{},healthy{};
    srhd_awa::matrixgame::native_form::ScreenshotFrame screenshot_frame;
    Callback primary_readback,presentation_readback;
    bool Healthy()const noexcept{return healthy;}
};
struct Device {
    std::unique_ptr<State> impl_=std::make_unique<State>();
    DeviceResult AbortNativeFrame()noexcept {
#include "abort-readback-prefix.inc"
        return 0; // The remainder talks to SDK; deliberately outside this test.
    }
};
int main(int argc,char**){
    const bool baseline=argc>1;unsigned cases{},late_calls{},retained{};
    for(unsigned health=0;health<3;++health)for(unsigned mask=0;mask<4;++mask){
        Device device;auto& state=*device.impl_;state.queue=health!=0;state.healthy=health==2;
        auto primary=std::make_shared<int>(1),presented=std::make_shared<int>(2);
        std::weak_ptr<int> primary_lifetime=primary,presentation_lifetime=presented;
        unsigned calls{};
        if(mask&1)state.primary_readback=[owner=primary,&calls](auto,unsigned,unsigned){assert(*owner==1);++calls;};
        if(mask&2)state.presentation_readback=[owner=presented,&calls](auto,unsigned,unsigned){assert(*owner==2);++calls;};
        primary.reset();presented.reset();
        assert(device.AbortNativeFrame()==(health==2?0:presentation::DeviceLost));
        assert(!calls && presentation_lifetime.expired() && !state.presentation_readback);
        if(baseline && (mask&1)){
            assert(!primary_lifetime.expired() && state.primary_readback);++retained;
            // Simulate later delivery with the retained request still queued.
            auto callback=std::move(state.primary_readback);state.primary_readback={};callback({},1,1);late_calls+=calls;
        }else assert(primary_lifetime.expired() && !state.primary_readback);
        assert(primary_lifetime.expired());
        assert(device.AbortNativeFrame()==(health==2?0:presentation::DeviceLost));
        // Fresh requests after cancellation remain possible and one-shot.
        state.primary_readback=[&calls](auto,unsigned,unsigned){++calls;};
        auto next=std::move(state.primary_readback);state.primary_readback={};
        auto before=calls;next({},1,1);assert(calls==before+1 && !state.primary_readback);
        ++cases;
    }
    assert(retained==(baseline?6u:0u) && late_calls==retained);
    std::printf("READBACK_ABORT %s cases=%u retained_primary=%u late_deliveries=%u actual_production_prefix CPU_CALLBACK_OWNERSHIP_ONLY NO_GPU NO_SWITCH\n",baseline?"BASELINE_REPRODUCED":"PASS",cases,retained,late_calls);
}
