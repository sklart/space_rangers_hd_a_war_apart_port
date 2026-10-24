#ifdef NDEBUG
#error AI owner verification requires assertions
#endif
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "Logic/MatrixEnvironment.h"
#include <cassert>
#include <cstdio>
#include <stdexcept>
using namespace srhd_awa::matrixgame;
void VerifyBootstrapAiOwners(NativeInitRuntime& runtime) {
    assert(runtime.CpuReady() && !runtime.GpuReady() && !g_MatrixMap);
    for(int cycle=0;cycle<2;++cycle) {
        CInfo info;assert(info.m_BadCoordCnt==0 && info.m_BadPlaceCnt==0 && !info.m_FirstEnemy && !info.m_LastEnemy);
        for(int i=0;i<17;++i)info.AddBadCoord({100+i,-200-i});
        assert(info.m_BadCoordCnt==16);
        for(int i=0;i<16;++i) {
            const CPoint expected{101+i,-201-i};const auto actual=info.m_BadCoord[i];
            if(actual!=expected) {
                std::printf("original CInfo coordinate FIFO slot=%d expected=(%d,%d) actual=(%d,%d) point_bytes=%zu\n",i,expected.x,expected.y,actual.x,actual.y,sizeof(CPoint));
                throw std::runtime_error("original CInfo coordinate FIFO corrupt after overflow");
            }
        }
        assert(!info.IsBadCoord({100,-200}) && info.IsBadCoord({116,-216}));
        info.AddBadCoord({116,-216});assert(info.m_BadCoordCnt==16);
        for(int i=17;i<40;++i)info.AddBadCoord({100+i,-200-i});
        for(int i=0;i<16;++i)assert(info.m_BadCoord[i]==CPoint(124+i,-224-i));
        for(int i=0;i<24;++i)info.AddBadPlace(1000+i);
        assert(info.m_BadPlaceCnt==8);
        for(int i=0;i<8;++i)assert(info.m_BadPlace[i]==1016+i);
        info.AddBadPlace(1023);assert(info.m_BadPlaceCnt==8 && !info.IsBadPlace(1015) && info.IsBadPlace(1023));
        info.Reset();assert(!info.m_Target && !info.m_TargetAttack);info.Clear();
    }
    std::puts("original full CInfo coordinate/place FIFO overflow, duplicate suppression and repeated lifecycle EXECUTED; enemy world and Side gameplay NOT EXECUTED");
}
