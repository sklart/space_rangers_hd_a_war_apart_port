"""Generate narrow Switch source overrides from the pinned, untouched upstream."""

from __future__ import annotations

from pathlib import Path


FUNCTIONS: dict[str, dict[str, str]] = {
    "DirectSound.cpp": {
        "std::int32_t PAS_STDCALL DirectSoundEnumerateA(TDSEnumCallback Callback, void* Context)":
            "static_cast<void>(Callback); static_cast<void>(Context);\n"
            "        return -1; // OPTIONAL: audio is disabled for the first Switch menu run.",
    },
    "WindowsImports.cpp": {
        "std::int32_t PAS_STDCALL CloseHandle(std::uint32_t Handle)":
            "return srhd_awa::platform::e2e_events::Close(Handle) ||\n"
            "            srhd_awa::platform::e2e_threads::Close(Handle) ? 1 : 0;",
        "std::uint32_t PAS_STDCALL GetLastError()":
            "return 0; // OPTIONAL: no Win32 last-error state exists on Switch.",
        "std::int32_t PAS_STDCALL QueryPerformanceCounter(std::int64_t& Counter)":
            "Counter = static_cast<std::int64_t>(srhd_awa::platform::e2e_clock::Counter());\n        return 1;",
        "std::uint32_t PAS_STDCALL GetTickCount()":
            "return srhd_awa::platform::e2e_clock::Milliseconds();",
        "std::uint32_t PAS_STDCALL GetCurrentThreadId()":
            "return srhd_awa::platform::e2e_threads::CurrentId();",
        "std::uint32_t PAS_STDCALL OpenEvent(std::uint32_t DesiredAccess, std::int32_t InheritHandle, std::uint8_t* Name)":
            "static_cast<void>(DesiredAccess); static_cast<void>(InheritHandle); static_cast<void>(Name);\n"
            "        return 0; // OPTIONAL: application lifecycle already enforces one instance.",
        "std::uint32_t PAS_STDCALL CreateEvent(void* Attributes, std::int32_t ManualReset, std::int32_t InitialState, std::uint8_t* Name)":
            "static_cast<void>(Attributes); static_cast<void>(Name);\n"
            "        return srhd_awa::platform::e2e_events::Create(ManualReset != 0, InitialState != 0);",
        "std::uint8_t* PAS_STDCALL GetCommandLineA()":
            "static std::uint8_t line[] = \"Rangers\";\n        return line;",
        "std::uint8_t* PAS_STDCALL CharNext(std::uint8_t* P)":
            "return P != nullptr && *P != 0 ? P + 1 : P; // Switch command line is ASCII.",
        "std::uint32_t PAS_STDCALL GetModuleFileNameA(std::uint32_t Module, std::uint8_t* FileName, std::uint32_t Capacity)":
            "static_cast<void>(Module); static_cast<void>(FileName); static_cast<void>(Capacity);\n"
            "        return 0; // E2E entrypoint sets the real game directory explicitly.",
    },
    "WindowsSdk.cpp": {
        "THandle PAS_STDCALL CreateThread(void* lpThreadAttributes, std::uint32_t dwStackSize, TFNThreadStartRoutine lpStartAddress, void* lpParameter, std::uint32_t dwCreationFlags, std::uint32_t& lpThreadId)":
            "static_cast<void>(lpThreadAttributes); static_cast<void>(dwStackSize);\n"
            "        return srhd_awa::platform::e2e_threads::Create(\n"
            "            reinterpret_cast<srhd_awa::platform::e2e_threads::Entry>(lpStartAddress),\n"
            "            lpParameter, (dwCreationFlags & CREATE_SUSPENDED) != 0, &lpThreadId);",
        "THandle PAS_STDCALL GetCurrentThread()":
            "return srhd_awa::platform::e2e_threads::CurrentId();",
        "BOOL PAS_STDCALL SetThreadPriority(THandle hThread, std::int32_t nPriority)":
            "return srhd_awa::platform::e2e_threads::SetPriority(hThread, nPriority) ? 1 : 0;",
        "std::int32_t PAS_STDCALL GetThreadPriority(THandle hThread)":
            "return srhd_awa::platform::e2e_threads::GetPriority(hThread);",
        "std::uint32_t PAS_STDCALL ResumeThread(THandle hThread)":
            "return srhd_awa::platform::e2e_threads::Resume(hThread);",
        "BOOL PAS_STDCALL SetEvent(THandle hEvent)":
            "return srhd_awa::platform::e2e_events::Set(hEvent) ? 1 : 0;",
        "BOOL PAS_STDCALL ResetEvent(THandle hEvent)":
            "return srhd_awa::platform::e2e_events::Reset(hEvent) ? 1 : 0;",
        "std::uint32_t PAS_STDCALL WaitForSingleObject(THandle hHandle, std::uint32_t dwMilliseconds)":
            "const auto event_result = srhd_awa::platform::e2e_events::WaitOne(hHandle, dwMilliseconds);\n"
            "        return event_result == srhd_awa::platform::e2e_events::kWaitFailed\n"
            "            ? srhd_awa::platform::e2e_threads::Wait(hHandle, dwMilliseconds) : event_result;",
        "std::uint32_t PAS_STDCALL WaitForMultipleObjects(std::uint32_t nCount, PWOHandleArray lpHandles, BOOL bWaitAll, std::uint32_t dwMilliseconds)":
            "return srhd_awa::platform::e2e_events::WaitMany(lpHandles ? lpHandles->elements : nullptr,\n"
            "            nCount, bWaitAll != 0, dwMilliseconds);",
        "BOOL PAS_STDCALL QueryPerformanceFrequency(Windows::TLargeInteger& lpFrequency)":
            "lpFrequency = static_cast<Windows::TLargeInteger>(srhd_awa::platform::e2e_clock::Frequency());\n        return lpFrequency > 0;",
    },
    "SysUtilsImports.cpp": {
        "void PAS_STDCALL Sleep(std::uint32_t Milliseconds)":
            "srhd_awa::platform::e2e_clock::SleepMilliseconds(Milliseconds);",
        "pas::AnsiString GetCurrentDir()":
            "std::error_code error;\n"
            "        const auto path = std::filesystem::current_path(error);\n"
            "        return error ? pas::AnsiString() : pas::AnsiString(path.generic_string().c_str());",
        "std::uint8_t SetCurrentDir(const pas::AnsiString& Dir)":
            "std::error_code error;\n"
            "        std::filesystem::current_path(Dir.c_str(), error);\n"
            "        return !error;",
    },
    "MMSystem.cpp": {
        "std::uint32_t PAS_STDCALL timeBeginPeriod(std::uint32_t Period)":
            "static_cast<void>(Period);\n        return 0; // OPTIONAL: Switch owns timer resolution.",
        "std::uint32_t PAS_STDCALL timeEndPeriod(std::uint32_t Period)":
            "static_cast<void>(Period);\n        return 0; // OPTIONAL: Switch owns timer resolution.",
        "std::uint32_t PAS_STDCALL timeGetTime()":
            "return srhd_awa::platform::e2e_clock::Milliseconds();",
        "std::uint32_t PAS_STDCALL timeKillEvent(std::uint32_t TimerId)":
            "static_cast<void>(TimerId);\n"
            "        return 0; // OPTIONAL: audio timers are disabled for the first menu run.",
        "std::uint32_t PAS_STDCALL timeSetEvent(std::uint32_t Delay, std::uint32_t Resolution, TFNTimeCallBack Callback, std::uint32_t User, std::uint32_t Flags)":
            "static_cast<void>(Delay); static_cast<void>(Resolution); static_cast<void>(Callback);\n"
            "        static_cast<void>(User); static_cast<void>(Flags);\n"
            "        return 0; // OPTIONAL: audio timers are disabled for the first menu run.",
    },
}

STAGES = {
    "Forms::UnitInitialize();": "Forms UnitInitialize",
    "ExceptionInfo::UnitInitialize();": "ExceptionInfo",
    "CheatCode::UnitInitialize();": "CheatCode",
    "Forms::TApplication::Initialize();": "Application Initialize",
    "GR_Main::InitializePlatformRuntimeAndMainWindow();": "platform runtime",
    "GR_Main::LoadLanguageAndPackages();": "language/packages",
    "Globals::InitializeScriptHostRuntime();": "script host",
    "GR_Main::InitializeRuntimeAndSettings();": "runtime/settings",
    "Globals::InitializeGlobalUiRuntime();": "global UI",
    "Robot::InitializeRobotRuntime();": "robot runtime",
    "Globals::RunMainScreenStateLoop();": "entering RunMainScreenStateLoop",
}

PROGRAM_SOURCE = '''#include "e2e_stage.hpp"
#include "ec_file_adapter.hpp"
#include "user_root.hpp"
#include "units/Rangers.hpp"
#include "units/GlobalsV.hpp"

#include <filesystem>
#include <exception>
#include <string>

int main() {
    namespace platform = srhd_awa::platform;
    platform::user_root::SetRoot(platform::e2e_stage::kRoot);
    platform::ec_file::SetGameRoot(std::string(platform::e2e_stage::kRoot) + "/game");
    platform::ec_file::SetUserRoot(platform::e2e_stage::kRoot);
    if (!platform::user_root::EnsureLayout()) {
        platform::e2e_stage::Log("FAIL stage=game-root user-layout");
        return 1;
    }
    std::error_code error;
    std::filesystem::current_path(
        std::filesystem::path(platform::e2e_stage::kRoot) / "game", error);
    if (error) {
        platform::e2e_stage::Log("FAIL stage=game-root current-directory");
        return 1;
    }
    platform::e2e_stage::Log("build_git=BUILD_GIT_PLACEHOLDER");
    try {
        Rangers::ProgramMain();
    } catch (const pas::Raised& error) {
        const auto* game_error = pas::class_cast_if<pas::Exception*>(error.object.get());
        std::string detail = "FAIL stage=ProgramMain exception=";
        detail += game_error ? game_error->message.c_str() : error.what();
        detail += " current=" + std::to_string(static_cast<unsigned>(GlobalsV::CurrentScreenId));
        detail += " requested=" + std::to_string(static_cast<unsigned>(GlobalsV::RequestedScreenId));
        detail += " post-load=" + std::to_string(static_cast<unsigned>(GlobalsV::PostLoadScreenId));
        platform::e2e_stage::Log(detail.c_str());
        return 1;
    } catch (const std::exception& error) {
        const std::string detail = std::string("FAIL stage=ProgramMain exception=") + error.what();
        platform::e2e_stage::Log(detail.c_str());
        return 1;
    } catch (...) {
        platform::e2e_stage::Log("FAIL stage=ProgramMain unhandled-exception");
        return 1;
    }
    platform::e2e_stage::Log("BOOT COMPLETE");
    return 0;
}
'''


def generated_source(source: Path, destination: Path, build_git: str = "") -> Path:
    overrides = FUNCTIONS.get(source.name)
    if not overrides and source.name not in ("Rangers.cpp", "GR_Main.cpp", "program.cpp"):
        return source
    text = PROGRAM_SOURCE if source.name == "program.cpp" else source.read_text(encoding="utf-8")
    if source.name == "program.cpp":
        if not build_git:
            raise RuntimeError("E2E build Git identity is missing")
        text = text.replace("BUILD_GIT_PLACEHOLDER", build_git)
    for signature, body in (overrides or {}).items():
        start = "    " + signature + " {"
        if text.count(start) != 1:
            raise RuntimeError(f"override signature changed: {source.name}: {signature}")
        begin = text.index(start)
        end = text.index("\n    }", begin) + len("\n    }")
        replacement = start + "\n        " + body + "\n    }"
        text = text[:begin] + replacement + text[end:]
    if source.name == "SysUtilsImports.cpp":
        first = "    std::uint8_t FileExists(const pas::AnsiString& FileName) {"
        following = "    std::uint8_t DirectoryExists(const pas::AnsiString& Directory) {"
        if text.count(first) != 1 or text.count(following) != 1:
            raise RuntimeError("original FileExists boundary changed")
        begin = text.index(first)
        end = text.index(following, begin)
        portable = '''    std::uint8_t FileExists(const pas::AnsiString& FileName) {
        const std::string user_path = srhd_awa::platform::user_root::ResolveConfigPath(FileName.c_str());
        std::error_code error;
        if (!user_path.empty() && std::filesystem::is_regular_file(user_path, error) && !error) return true;
        return srhd_awa::platform::game_path::FileExists(FileName.c_str());
    }

'''
        text = text[:begin] + portable + text[end:]
        text = '#include "game_path.hpp"\n#include "user_root.hpp"\n#include <filesystem>\n' + text
    if overrides:
        text = '#include "e2e_clock.hpp"\n' + text
    if source.name in ("WindowsImports.cpp", "WindowsSdk.cpp"):
        text = '#include "e2e_events.hpp"\n#include "e2e_threads.hpp"\n' + text
    if source.name == "Rangers.cpp":
        registry_line = next((line for line in text.splitlines()
                              if "EC_Str::WriteRegistryStringLegacy(WindowsImports::HKEY_LOCAL_MACHINE" in line), None)
        if registry_line is None:
            raise RuntimeError("optional registry startup call changed")
        text = text.replace(registry_line,
            "#if !defined(__SWITCH__)\n" + registry_line + "\n#endif", 1)
        wine_begin = '                                        WineModule = WindowsImports::LoadLibrary(pas::literal_pointer("ntdll.dll"));'
        wine_end = '                                            WindowsImports::FreeLibrary(WineModule);\n                                        }'
        if text.count(wine_begin) != 1 or text.count(wine_end) != 1:
            raise RuntimeError("optional Wine detection block changed")
        text = text.replace(wine_begin, "#if !defined(__SWITCH__)\n" + wine_begin, 1)
        text = text.replace(wine_end, wine_end + "\n#endif", 1)
        steam_load = "                                                SimpleSteamApi::LoadSteamApi();"
        steam_init = "                                                SimpleSteamApi::SteamInitialized = SimpleSteamApi::SteamInit(GR_Main::SelectedLanguage, GR_Main::AvailableLanguageCodes);"
        if text.count(steam_load) != 1 or text.count(steam_init) != 1:
            raise RuntimeError("optional Steam startup block changed")
        text = text.replace(steam_load, "#if !defined(__SWITCH__)\n" + steam_load, 1)
        text = text.replace(steam_init, steam_init + "\n#endif", 1)
        callback_begin = "                                            if (SteamCallbackThread == nullptr) {"
        callback_end = "                                                SteamCallbackThread->Start();\n                                            }"
        if text.count(callback_begin) != 1 or text.count(callback_end) != 1:
            raise RuntimeError("optional Steam callback block changed")
        text = text.replace(callback_begin, "#if !defined(__SWITCH__)\n" + callback_begin, 1)
        text = text.replace(callback_end, callback_end + "\n#endif", 1)
        steam_shutdown = "                                        SteamCallbackThread->RequestStop();\n                                        SimpleSteamApi::UnloadSteamApi();"
        if text.count(steam_shutdown) != 1:
            raise RuntimeError("optional Steam shutdown block changed")
        text = text.replace(steam_shutdown,
            "#if !defined(__SWITCH__)\n" + steam_shutdown + "\n#endif", 1)
        for call, stage in STAGES.items():
            if text.count(call) != 1:
                raise RuntimeError(f"startup stage changed: {call}")
            text = text.replace(call, f'srhd_awa::platform::e2e_stage::Log("{stage}");\n                                        {call}')
        text = text.replace("void ProgramMain() {", "void ProgramMain() {\n        srhd_awa::platform::e2e_stage::Log(\"ProgramMain BEGIN\");", 1)
        text = '#include "e2e_stage.hpp"\n' + text
    if source.name == "GR_Main.cpp":
        geometry_begin = "    void ApplyMainWindowGeometry() {"
        focus_begin = "    void ShowAndFocusMainWindow() {"
        dat_begin = "    void LoadDatConfigAndModOverrides() {"
        if any(text.count(marker) != 1 for marker in (geometry_begin, focus_begin, dat_begin)):
            raise RuntimeError("original SDL window adaptation boundary changed")
        begin = text.index(geometry_begin)
        end = text.index(dat_begin, begin)
        text = text[:begin] + '''    void ApplyMainWindowGeometry() {
        auto* window = static_cast<SDL_Window*>(g_e2e_platform.native_window);
        if (window != nullptr) {
            SDL_SetWindowSize(window, std::max(1, PresentationWidth), std::max(1, PresentationHeight));
        }
    }

    void ShowAndFocusMainWindow() {
        auto* window = static_cast<SDL_Window*>(g_e2e_platform.native_window);
        if (window == nullptr) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=runtime/settings SDL-window-missing");
            pas::raise(pas::make_exception<pas::Exception>("Switch SDL window unavailable"_a));
        }
        SDL_ShowWindow(window);
        SDL_RaiseWindow(window);
    }

''' + text[end:]
        init_begin = "    void InitializeRuntimeAndSettings() {"
        registry_begin = "        Text = EC_Str::TrimWideString(GR_Main::ReadRegistryText(WindowsImports::HKEY_LOCAL_MACHINE"
        settings_begin = "        UserSettingsConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);"
        if text.count(init_begin) != 1 or registry_begin not in text or text.count(settings_begin) != 1:
            raise RuntimeError("original runtime/settings platform diagnostics boundary changed")
        begin = text.index(registry_begin, text.index(init_begin))
        end = text.index(settings_begin, begin)
        text = text[:begin] + '''        ProcessorCoreCount = std::max<std::int32_t>(1, std::thread::hardware_concurrency());
        GR_Main::AppendLogLineThreadSafe("Operating System=Nintendo Switch"_a);
        GR_Main::AppendLogLineThreadSafe(pas::concat_ansi({"Processor cores=", SysUtils::IntToStr(ProcessorCoreCount)}));
        MemoryStatus.Length = static_cast<std::int32_t>(sizeof(TMemoryStatusEx));
        if (!GR_Main::GlobalMemoryStatusEx(MemoryStatus)) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=runtime/settings memory-info");
            pas::raise(pas::make_exception<pas::Exception>("Switch process memory info unavailable"_a));
        }
''' + text[end:]
        memory_begin = "    std::int32_t PAS_STDCALL GlobalMemoryStatusEx(TMemoryStatusEx& Status) {"
        memory_end = "    void* OKGF_MulTable256x256() {"
        if text.count(memory_begin) != 1 or text.count(memory_end) != 1:
            raise RuntimeError("original process memory import boundary changed")
        begin = text.index(memory_begin)
        end = text.index(memory_end, begin)
        text = text[:begin] + '''    std::int32_t PAS_STDCALL GlobalMemoryStatusEx(TMemoryStatusEx& Status) {
        u64 total = 0;
        u64 used = 0;
        if (R_FAILED(svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0)) ||
            R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)) ||
            total == 0 || used > total) {
            return 0;
        }
        Status.TotalPhys = total;
        Status.AvailPhys = total - used;
        Status.TotalPageFile = total;
        Status.AvailPageFile = total - used;
        Status.TotalVirtual = total;
        Status.AvailVirtual = total - used;
        Status.MemoryLoad = static_cast<std::uint32_t>(used * 100 / total);
        return 1;
    }

''' + text[end:]
        config_copy = '            WindowsSdk::CopyFileW(pas::literal_pointer(u"cfg.txt"), Text.pchar(), 0);'
        if text.count(config_copy) != 1:
            raise RuntimeError("original CFG.TXT creation boundary changed")
        text = text.replace(config_copy, '''            std::error_code copy_error;
            const auto cfg_source = srhd_awa::platform::game_path::Resolve("cfg.txt");
            const auto cfg_target = srhd_awa::platform::user_root::ResolveConfigPath("CFG.TXT");
            if (cfg_source.empty() || cfg_target.empty() ||
                !std::filesystem::copy_file(cfg_source, cfg_target,
                    std::filesystem::copy_options::overwrite_existing, copy_error) || copy_error) {
                srhd_awa::platform::e2e_stage::Log("FAIL stage=runtime/settings copy-cfg");
                pas::raise(pas::make_exception<pas::Exception>("Switch CFG.TXT copy failed"_a));
            }''', 1)
        affinity = "        GR_Main::ApplyProcessAffinity();"
        if text.count(affinity) != 1:
            raise RuntimeError("original optional process-affinity call changed")
        text = text.replace(affinity, "        // OPTIONAL: Switch owns process CPU affinity.", 1)
        show_cursor = "            WindowsSdk::ShowCursor(0);"
        if text.count(show_cursor) != 1:
            raise RuntimeError("original cursor-visibility call changed")
        text = text.replace(show_cursor, "            SDL_ShowCursor(SDL_DISABLE);", 1)
        dll_checksum_begin = '        Text = u"ll"_w;'
        dll_checksum_end = "        CCInterface->SetResourceChecksumFailed(SavedChecksumFailed);"
        if text.count(dll_checksum_begin) != 1 or text.count(dll_checksum_end) != 1:
            raise RuntimeError("original Windows DLL integrity boundary changed")
        text = text.replace(dll_checksum_begin,
            "#if !defined(__SWITCH__)\n" + dll_checksum_begin, 1)
        text = text.replace(dll_checksum_end,
            "#endif // Windows DLL integrity paths are absent on Switch.\n" + dll_checksum_end, 1)
        audio_anchor = '        EC_BlockPar::TBlockParEC* Block = LanguageDataConfig->GetBlock(u"CaseConv"sv);'
        if text.count(audio_anchor) != 1:
            raise RuntimeError("original sound initialization boundary changed")
        text = text.replace(audio_anchor,
            "        // OPTIONAL: the first Switch menu run has no audio backend.\n"
            "        GlobalsV::SoundEnabled = false;\n"
            "        GlobalsV::MusicEnabled = false;\n" + audio_anchor, 1)
        begin_marker = "    void InitializePlatformRuntimeAndMainWindow() {"
        tail_marker = "        InstallConfig = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);"
        if text.count(begin_marker) != 1 or text.count(tail_marker) != 1:
            raise RuntimeError("original GR_Main platform initialization boundary changed")
        begin = text.index(begin_marker)
        tail = text.index(tail_marker, begin)
        platform_prefix = '''    void InitializePlatformRuntimeAndMainWindow() {
        std::string error;
        if (!srhd_awa::platform::runtime_platform::InitializePlatformServices(&g_e2e_platform, &error)) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=platform-runtime SDL-services");
            pas::raise(pas::make_exception<pas::Exception>("Switch SDL services unavailable"_a));
        }
        PerformanceCounterFrequency = static_cast<std::int64_t>(srhd_awa::platform::e2e_clock::Frequency());
        srhd_awa::platform::e2e_stage::Log("platform runtime SDL services ready");
        if (!aPacket::InitializePackageCollection()) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=platform-runtime packages");
            pas::raise(pas::make_exception<pas::Exception>("Switch package collection unavailable"_a));
        }
        srhd_awa::platform::e2e_stage::Log("platform runtime package collection ready");
        if (!srhd_awa::platform::runtime_platform::CreateMainWindow(&g_e2e_platform, &error)) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=platform-runtime SDL-window");
            pas::raise(pas::make_exception<pas::Exception>("Switch SDL window unavailable"_a));
        }
        srhd_awa::platform::e2e_stage::Log("platform runtime SDL window ready");
        MainWindowHandle = g_e2e_platform.window_token;
        Forms::Application->Handle = MainWindowHandle;
        srhd_awa::platform::renderer_platform::SetNativeWindow(g_e2e_platform.native_window);
        GR_Main::AppendLogLineThreadSafe("Build=2.1.2500 (Switch E2E)"_a);
        srhd_awa::platform::e2e_stage::Log("platform runtime session log ready");
'''
        text = text[:begin] + platform_prefix + text[tail:]
        text = text.replace(tail_marker, tail_marker + '\n'
            '        srhd_awa::platform::e2e_stage::Log("platform runtime install parser ready");', 1)
        load_install = '        InstallConfig->LoadFromTextFileWithEncodingProbe(pas::literal_pointer(u"install.txt"), false);'
        if text.count(load_install) != 1:
            raise RuntimeError("original install.txt loading boundary changed")
        text = text.replace(load_install, load_install + '\n'
            '        srhd_awa::platform::e2e_stage::Log("platform runtime install.txt loaded");', 1)
        destroy = "        WindowsSdk::DestroyWindow(MainWindowHandle);"
        uninitialize = "        ActiveXSdk::CoUninitialize();"
        if text.count(destroy) != 1 or text.count(uninitialize) != 1:
            raise RuntimeError("original GR_Main platform finalization boundary changed")
        text = text.replace(destroy,
            "        srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);\n"
            "        srhd_awa::platform::runtime_platform::ShutdownPlatformServices(&g_e2e_platform);", 1)
        text = text.replace(uninitialize,
            "        // OPTIONAL: no COM apartment exists in the Switch SDL runtime.", 1)
        text = ('#include "runtime_platform.hpp"\n#include "renderer_platform.hpp"\n'
                '#include "e2e_clock.hpp"\n#include "e2e_stage.hpp"\n'
                '#include "game_path.hpp"\n#include "user_root.hpp"\n'
                '#include <SDL2/SDL.h>\n#include <switch.h>\n#include <thread>\n'
                'namespace { srhd_awa::platform::runtime_platform::State g_e2e_platform; }\n' + text)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")
    return destination
