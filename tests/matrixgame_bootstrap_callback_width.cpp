#ifdef NDEBUG
#error Callback width verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "Effects/MatrixEffect.hpp"
#include "Effects/MatrixEffectMovingObject.hpp"
#include "moving_object_callbacks.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
using namespace srhd_awa::matrixgame;
namespace {
struct Payload{std::uint64_t marker;};
struct Observation{uintptr_t expected{},observed{};unsigned calls{};bool valid{};};
Observation* active;
void Capture(CMatrixMapStatic* object,const D3DXVECTOR3& position,uintptr_t user,std::uint32_t flags){
    assert(active && !object && position.x==4 && position.y==6 && position.z==8 && flags==0);
    ++active->calls;active->observed=user;
    // A broken baseline must report truncation, not dereference a bad pointer.
    if(user==active->expected)active->valid=reinterpret_cast<Payload*>(user)->marker==0xf17ac0de12345678ULL;
}
}
void VerifyBootstrapCallbackWidth(NativeInitRuntime& runtime){
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    const float distance=CMatrixEffect::m_Dist2;
    struct Restore{float value;~Restore(){CMatrixEffect::m_Dist2=value;active=nullptr;}} restore{distance};
    for(int repeat=0;repeat<2;++repeat){
        Payload payload{0xf17ac0de12345678ULL};Observation result{reinterpret_cast<uintptr_t>(&payload)};active=&result;
        if(sizeof(uintptr_t)==8)assert(result.expected>0xffffffffULL);
        SMOProps properties;properties.startpos={1,2,3};properties.endhandler=Capture;properties.uservalue=result.expected;
        assert(moving_object::InvokeHit({4,6,8},nullptr,reinterpret_cast<uintptr_t>(&properties)));
        std::printf("original MOEnum callback pointer expected=%llx observed=%llx storage=%zu distance=%g\n",
            static_cast<unsigned long long>(result.expected),static_cast<unsigned long long>(result.observed),sizeof(properties.uservalue),CMatrixEffect::m_Dist2);
        assert(result.calls==1 && CMatrixEffect::m_Dist2==50);
        if(!result.valid)throw std::runtime_error("moving-object callback payload truncated");
        properties.endhandler=nullptr;
        assert(moving_object::InvokeHit({4,6,8},nullptr,reinterpret_cast<uintptr_t>(&properties)) && result.calls==1);
    }
    std::puts("original moving-object enum callback actual high-address payload round trip and null-handler branch EXECUTED; no fabricated map/hit/weapon battle");
}
