// Compile against the pinned MatrixGame declarations with the Windows x86 SDK.
// This proves boundary layout only; it does not launch or simulate a battle.
#include <windows.h>
#include "MatrixGameDll.hpp"
#include <cstddef>
#include <cstdio>
#include <type_traits>

static_assert(sizeof(void*) == 4, "Run this probe with a Windows x86 compiler");
static_assert(sizeof(long) == 4 && sizeof(dword) == 4 && sizeof(wchar) == 2);
static_assert(sizeof(bool) == 1);
static_assert(sizeof(SRobotsSettings) == 56 && alignof(SRobotsSettings) == 4);
static_assert(offsetof(SRobotsSettings, m_ShowStencilShadows) == 8);
static_assert(offsetof(SRobotsSettings, m_BPP) == 16);
static_assert(offsetof(SRobotsSettings, m_Brightness) == 32);
static_assert(offsetof(SRobotsSettings, m_MaxDistance) == 48);
static_assert(offsetof(SRobotsSettings, m_VSync) == 52);
static_assert(sizeof(SRobotGameState) == 24 && alignof(SRobotGameState) == 4);
static_assert(offsetof(SRobotGameState, m_KillBuilding) == 20);
static_assert(sizeof(SMGDRobotInterface) == 16);
static_assert(offsetof(SMGDRobotInterface, m_Run) == 12);
static_assert(sizeof(SMGDRangersInterface) == 64);
static_assert(offsetof(SMGDRangersInterface, m_RangersText) == 36);
static_assert(offsetof(SMGDRangersInterface, m_ProgressBar) == 44);
static_assert(offsetof(SMGDRangersInterface, m_MusicVolumeSet) == 60);
static_assert(sizeof(SMGDRangersInterfaceText) == 20);
static_assert(offsetof(SMGDRangersInterfaceText, m_Buf) == 4);
using Export = SMGDRobotInterface*(__cdecl*)();
using Run = int(__stdcall*)(HINSTANCE, HWND, wchar*, SRobotsSettings*, wchar*,
                            wchar*, wchar*, wchar*, wchar*, SRobotGameState*);
using SoundCreate = dword(__stdcall*)(wchar*, int, int);
static_assert(std::is_same_v<decltype(&GetRobotInterface), Export>);
static_assert(std::is_same_v<decltype(SMGDRobotInterface::m_Run), Run>);
static_assert(std::is_same_v<decltype(SMGDRangersInterface::m_SoundCreate), SoundCreate>);

int main(int argc, char** argv) {
    std::printf("{\"scope\":\"PINNED MATRIXGAME WINDOWS X86 ABI ONLY\","
                "\"pointer_bytes\":%zu,\"long_bytes\":%zu,\"wchar_bytes\":%zu,"
                "\"settings_bytes\":%zu,\"statistics_bytes\":%zu,"
                "\"robot_interface_bytes\":%zu,\"callbacks_bytes\":%zu,"
                "\"text_image_bytes\":%zu,\"battle_executed\":false}\n",
                sizeof(void*), sizeof(long), sizeof(wchar), sizeof(SRobotsSettings),
                sizeof(SRobotGameState), sizeof(SMGDRobotInterface),
                sizeof(SMGDRangersInterface), sizeof(SMGDRangersInterfaceText));
    if (argc > 1) {
        const HMODULE module = LoadLibraryExA(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) {
            std::fprintf(stderr, "MatrixGame DLL load failed win32_error=%lu\n", GetLastError());
            return 2;
        }
        const auto query = reinterpret_cast<Export>(GetProcAddress(module, "GetRobotInterface"));
        const auto* table = query ? query() : nullptr;
        const bool valid = table && table->m_Init && table->m_Deinit &&
                           table->m_Support && table->m_Run;
        std::printf("{\"scope\":\"REAL X86 DLL EXPORT TABLE ONLY\","
                    "\"export_table_valid\":%s,\"battle_executed\":false}\n",
                    valid ? "true" : "false");
        FreeLibrary(module);
        return valid ? 0 : 3;
    }
}
