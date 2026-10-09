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
        "std::int32_t PAS_STDCALL QueryPerformanceCounter(std::int64_t& Counter)":
            "Counter = static_cast<std::int64_t>(srhd_awa::platform::e2e_clock::Counter());\n        return 1;",
        "std::uint32_t PAS_STDCALL GetTickCount()":
            "return srhd_awa::platform::e2e_clock::Milliseconds();",
        "std::uint32_t PAS_STDCALL GetCurrentThreadId()":
            "return static_cast<std::uint32_t>(threadGetCurHandle());",
        "std::uint32_t PAS_STDCALL OpenEvent(std::uint32_t DesiredAccess, std::int32_t InheritHandle, std::uint8_t* Name)":
            "static_cast<void>(DesiredAccess); static_cast<void>(InheritHandle); static_cast<void>(Name);\n"
            "        return 0; // OPTIONAL: application lifecycle already enforces one instance.",
        "std::uint32_t PAS_STDCALL CreateEvent(void* Attributes, std::int32_t ManualReset, std::int32_t InitialState, std::uint8_t* Name)":
            "static_cast<void>(Attributes); static_cast<void>(ManualReset); static_cast<void>(InitialState); static_cast<void>(Name);\n"
            "        return 1; // OPTIONAL: no interprocess event is needed on Switch.",
        "std::uint8_t* PAS_STDCALL GetCommandLineA()":
            "static std::uint8_t line[] = \"Rangers\";\n        return line;",
        "std::uint32_t PAS_STDCALL GetModuleFileNameA(std::uint32_t Module, std::uint8_t* FileName, std::uint32_t Capacity)":
            "static_cast<void>(Module); static_cast<void>(FileName); static_cast<void>(Capacity);\n"
            "        return 0; // E2E entrypoint sets the real game directory explicitly.",
    },
    "WindowsSdk.cpp": {
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

#include <filesystem>
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
        if (!aPacket::InitializePackageCollection()) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=platform-runtime packages");
            pas::raise(pas::make_exception<pas::Exception>("Switch package collection unavailable"_a));
        }
        if (!srhd_awa::platform::runtime_platform::CreateMainWindow(&g_e2e_platform, &error)) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=platform-runtime SDL-window");
            pas::raise(pas::make_exception<pas::Exception>("Switch SDL window unavailable"_a));
        }
        MainWindowHandle = g_e2e_platform.window_token;
        Forms::Application->Handle = MainWindowHandle;
        srhd_awa::platform::renderer_platform::SetNativeWindow(g_e2e_platform.native_window);
        GR_Main::AppendLogLineThreadSafe("Build=2.1.2500 (Switch E2E)"_a);
'''
        text = text[:begin] + platform_prefix + text[tail:]
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
                'namespace { srhd_awa::platform::runtime_platform::State g_e2e_platform; }\n' + text)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")
    return destination
