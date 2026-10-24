#ifdef NDEBUG
#error Cannon unit verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixMap.hpp"
#include "MatrixObjectCannon.hpp"
#include "Effects/MatrixEffectSelection.hpp"
#include "cannon_unit_lifecycle.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapCannonUnits(NativeInitRuntime& runtime) {
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    SMatrixCannonUnit units[MR_MAXCANNONUNIT];
    cannon_units::Reset(units,MR_MAXCANNONUNIT);
    for(int repeat=0;repeat<2;++repeat) {
        auto* effect=CMatrixEffect::CreateSelection({1,2,3},1,0x80abcdef);
        units[0].m_Smoke.effect=effect;effect->SetHandler(&units[0].m_Smoke);
        units[0].m_Type=CUT_SHAFT;units[0].m_Matrix._11=7;
        units[0].u1.s1.m_Angle=3;units[0].u1.s1.m_LinkMatrix=12;
        units[0].u1.s1.m_IMatrix._42=9;units[0].u1.s1.m_Invert=1;
        cannon_units::Reset(units,MR_MAXCANNONUNIT);
        // Original map ownership is unchanged: the actual effect survives,
        // but its backlink must not target a reset (or later destroyed) slot.
        assert(!effect->GetHandler() && effect->GetType()==EFFECT_SELECTION);
        for(const auto& unit:units) {
            assert(unit.m_Type==CUT_EMPTY && !unit.m_Graph && !unit.m_Smoke.effect);
            assert(!unit.u1.s1.m_ShadowStencil && unit.u1.s1.m_Angle==0 && unit.u1.s1.m_LinkMatrix==0 && !unit.u1.s1.m_Invert);
            for(int row=0;row<4;++row)for(int col=0;col<4;++col)assert(unit.m_Matrix.m[row][col]==0 && unit.u1.s1.m_IMatrix.m[row][col]==0);
        }
        effect->Release();
        cannon_units::Reset(units,MR_MAXCANNONUNIT);
    }
    std::puts("actual cannon units typed reset, original effect handler detach/survival/release/reentry EXECUTED; no fabricated map or cannon constructor");
}
