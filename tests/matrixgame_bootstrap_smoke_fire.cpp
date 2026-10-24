#ifdef NDEBUG
#error Smoke/fire integration verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "Effects/MatrixEffectSmokeAndFire.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;

void VerifyBootstrapSmokeFire(NativeInitRuntime& runtime){
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    const auto textures=legacy_texture::RegistrySize();
    const auto allocated=buffers::Current().AllocatedBytes();
    for(int repeat=0;repeat<2;++repeat){
        auto* effect=HNew(CMatrixEffect::m_Heap) CMatrixEffectFireStream({1,2,3},{4,5,6});
        SEffectHandler handler;
        handler.effect=effect;effect->SetHandler(&handler);
        assert(effect->GetType()==EFFECT_FIRESTREAM && effect->GetHandler()==&handler);
        effect->SetPos({-3,2,7},{9,1,4});effect->BeforeDraw();effect->Takt(16);
        assert(handler.effect==effect);
        // Original Release destroys the five original billboard lines, then
        // the original effect base clears its real handler backlink.
        effect->Release();assert(!handler.effect);
        assert(legacy_texture::RegistrySize()==textures && buffers::Current().AllocatedBytes()==allocated);
    }
    std::puts("original full SmokeAndFire integrated; FireStream constructor/SetPos/Release and handler cleanup/reentry EXECUTED; Smoke/Fire/FireAnim map factories and Draw NOT EXECUTED");
}
