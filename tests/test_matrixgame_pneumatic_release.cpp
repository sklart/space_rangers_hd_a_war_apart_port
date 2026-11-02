// Uses the real class declaration, allocator and extracted production method.
// No map, robot motion, Init or GPU behavior is simulated here.
#include "MatrixObjectRobot.hpp"
#include <cassert>
#include <cstdio>
#ifdef NDEBUG
#error Pneumatic release assertions must be enabled
#endif
Base::CHeap* g_MatrixHeap{};
SPneumaticData* CMatrixRobot::m_Pneumaic{};
int g_fcnt{};
struct PneumaticAccess: CMatrixRobot {
    static void Allocate() {
        assert(!m_Pneumaic);
        m_Pneumaic=static_cast<SPneumaticData*>(HAlloc(sizeof(SPneumaticData)*4,g_MatrixHeap));
        g_fcnt=2;
    }
    static bool Empty(){return !m_Pneumaic && !g_fcnt;}
};
int main(){
    CMatrixRobot::DestroyPneumaticData(); // Early Init failure before preload.
    PneumaticAccess::Allocate();
    CMatrixRobot::DestroyPneumaticData(); // Normal session teardown.
    CMatrixRobot::DestroyPneumaticData(); // Later failure before next preload.
    assert(PneumaticAccess::Empty());
    PneumaticAccess::Allocate();
    CMatrixRobot::DestroyPneumaticData();
    CMatrixRobot::DestroyPneumaticData();
    assert(PneumaticAccess::Empty());
    std::puts("PNEUMATIC_RELEASE PASS actual_class actual_allocator production_method null repeated_release reallocate CPU_ONLY NO_INIT NO_GPU");
}
