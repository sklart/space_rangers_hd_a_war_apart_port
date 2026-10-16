// Compile only with Windows x86: validates the pinned public header directly.
#include <windows.h>
#include "MatrixGameDll.hpp"
#include <cstddef>
static_assert(sizeof(void*) == 4 && sizeof(long) == 4 && sizeof(wchar_t) == 2);
static_assert(sizeof(SRobotsSettings) == 56);
static_assert(offsetof(SRobotsSettings, m_BPP) == 16);
static_assert(offsetof(SRobotsSettings, m_VSync) == 52);
static_assert(sizeof(SMGDRobotInterface) == 16);
static_assert(offsetof(SMGDRobotInterface, m_Run) == 12);
static_assert(sizeof(SMGDRangersInterface) == 64);
static_assert(sizeof(SMGDRangersInterfaceText) == 20);
static_assert(sizeof(SRobotGameState) == 24);
