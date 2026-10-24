#ifdef NDEBUG
#error Selection integration verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "Effects/MatrixEffect.hpp"
#include "Effects/MatrixEffectSelection.hpp"
#include "legacy_texture_backend.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapSelection(NativeInitRuntime& runtime){
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    const auto registry=legacy_texture::RegistrySize();const auto bytes=buffers::Current().AllocatedBytes();
    for(int order=0;order<2;++order){
        // Original private constructor is reached through its real factory.
        auto* first=static_cast<CMatrixEffectSelection*>(CMatrixEffect::CreateSelection({1,2,3},1,0x80abcdef));
        auto* second=static_cast<CMatrixEffectSelection*>(CMatrixEffect::CreateSelection({-2,4,7},25,0xff123456));
        SEffectHandler a,z;a.effect=first;first->SetHandler(&a);z.effect=second;second->SetHandler(&z);
        assert(first->GetType()==EFFECT_SELECTION && second->GetType()==EFFECT_SELECTION);
        assert(first->Priority()==MAX_EFFECT_PRIORITY && second->Priority()==MAX_EFFECT_PRIORITY);
        first->SetPos({4,-3,8});first->SetColor(0x77123456);first->SetAlpha(128);first->SetRadius(2);
        first->BeforeDraw();second->BeforeDraw();
        if(order){second->Release();assert(!z.effect && a.effect==first);first->Release();}
        else{first->Release();assert(!a.effect && z.effect==second);second->Release();}
        assert(!a.effect && !z.effect && !g_MatrixMap);
        assert(legacy_texture::RegistrySize()==registry && buffers::Current().AllocatedBytes()==bytes);
    }
    std::puts("original CreateSelection factory/constructor/setters/point cleanup/handler backlinks/both release orders/reentry EXECUTED; texture slots unloaded; world Takt and Draw NOT EXECUTED");
}
