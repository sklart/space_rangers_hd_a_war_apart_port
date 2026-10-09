"""Generate narrow Switch source overrides from the pinned, untouched upstream."""

from __future__ import annotations

from pathlib import Path


FUNCTIONS: dict[str, dict[str, str]] = {
    "DirectSound.cpp": {
        "std::int32_t PAS_STDCALL DirectSoundEnumerateA(TDSEnumCallback Callback, void* Context)":
            "static_cast<void>(Callback); static_cast<void>(Context);\n"
            "        return -1; // OPTIONAL: audio is disabled for the first Switch menu run.",
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
#include "win32_compat.hpp"
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
    platform::e2e_stage::LogWinApi("manifest=MANIFEST_SHA_PLACEHOLDER");
    platform::e2e_stage::LogWinApi("portable=PORTABLE_COUNT_PLACEHOLDER");
    platform::e2e_stage::LogWinApi("optional=OPTIONAL_COUNT_PLACEHOLDER");
    platform::e2e_stage::LogWinApi("unknown=UNKNOWN_COUNT_PLACEHOLDER");
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
        platform::win32_compat::LogRuntimeStats();
        return 1;
    } catch (const std::exception& error) {
        const std::string detail = std::string("FAIL stage=ProgramMain exception=") + error.what();
        platform::e2e_stage::Log(detail.c_str());
        platform::win32_compat::LogRuntimeStats();
        return 1;
    } catch (...) {
        platform::e2e_stage::Log("FAIL stage=ProgramMain unhandled-exception");
        platform::win32_compat::LogRuntimeStats();
        return 1;
    }
    platform::win32_compat::LogRuntimeStats();
    platform::e2e_stage::Log("BOOT COMPLETE");
    return 0;
}
'''


def generated_source(source: Path, destination: Path, build_git: str = "",
                     manifest_stamp: dict | None = None) -> Path:
    overrides = FUNCTIONS.get(source.name)
    if not overrides and source.name not in ("Rangers.cpp", "Globals.cpp", "GR_Main.cpp", "aSaveLoad.cpp", "program.cpp"):
        return source
    text = PROGRAM_SOURCE if source.name == "program.cpp" else source.read_text(encoding="utf-8")
    if source.name == "program.cpp":
        if not build_git or manifest_stamp is None:
            raise RuntimeError("E2E build Git identity or Win32 manifest stamp is missing")
        text = text.replace("BUILD_GIT_PLACEHOLDER", build_git)
        for placeholder, key in (("MANIFEST_SHA_PLACEHOLDER", "sha256"),
                                 ("PORTABLE_COUNT_PLACEHOLDER", "portable"),
                                 ("OPTIONAL_COUNT_PLACEHOLDER", "optional"),
                                 ("UNKNOWN_COUNT_PLACEHOLDER", "unknown")):
            text = text.replace(placeholder, str(manifest_stamp[key]))
    for signature, body in (overrides or {}).items():
        start = "    " + signature + " {"
        if text.count(start) != 1:
            raise RuntimeError(f"override signature changed: {source.name}: {signature}")
        begin = text.index(start)
        end = text.index("\n    }", begin) + len("\n    }")
        replacement = start + "\n        " + body + "\n    }"
        text = text[:begin] + replacement + text[end:]
    if overrides:
        text = '#include "e2e_clock.hpp"\n' + text
    if source.name == "Rangers.cpp":
        executable_begin = "                            ExecutableFileName.set_length(WindowsImports::MAX_PATH);"
        executable_end = "                            EC_Str::WriteRegistryStringLegacy(WindowsImports::HKEY_LOCAL_MACHINE"
        if text.count(executable_begin) != 1 or text.count(executable_end) != 1:
            raise RuntimeError("original executable-directory startup boundary changed")
        begin = text.index(executable_begin)
        end = text.index(executable_end, begin)
        text = (text[:begin] + "#if !defined(__SWITCH__)\n" + text[begin:end] +
                "#endif\n" + text[end:])
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
        script_done = "Globals::InitializeScriptHostRuntime();"
        text = text.replace(script_done, script_done + '\n                                        '
            'srhd_awa::platform::e2e_stage::Log("script host ready");', 1)
        startup_clock = "WindowsImports::GetSystemTime(StartupTime);"
        if text.count(startup_clock) != 1:
            raise RuntimeError("startup clock boundary changed")
        text = text.replace(startup_clock,
            'srhd_awa::platform::e2e_stage::Log("startup clock");\n                                        '
            + startup_clock, 1)
        text = text.replace("void ProgramMain() {", "void ProgramMain() {\n        srhd_awa::platform::e2e_stage::Log(\"ProgramMain BEGIN\");", 1)
        text = '#include "e2e_stage.hpp"\n' + text
    if source.name == "Globals.cpp":
        for call, stage in (
            ("aPath::InitializePathNodePool();", "script host path pool"),
            ("TurnCalculationThread = pas::construct_call<ThreadCalc::TThreadCalc>(EC_Thread::TThreadEC_Create);", "script host worker"),
            ("aScript::InitializeScriptEngine();", "script host engine"),
        ):
            if text.count(call) != 1:
                raise RuntimeError(f"script-host stage changed: {call}")
            text = text.replace(call,
                f'srhd_awa::platform::e2e_stage::Log("{stage}");\n        {call}', 1)
        engine_call = "aScript::InitializeScriptEngine();"
        text = text.replace(engine_call, engine_call + '\n        '
            'srhd_awa::platform::e2e_stage::Log("script host engine ready");', 1)
        text = '#include "e2e_stage.hpp"\n' + text
    if source.name == "aSaveLoad.cpp":
        replacements = {
            "std::uint32_t MemorySnapshotGalaxyToken{};":
                "std::uint32_t MemorySnapshotGalaxyToken{};\n"
                "    static std::uintptr_t MemorySnapshotGalaxyAddressToken{};",
            "MemorySnapshotGalaxyToken = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(aGalaxy::Galaxy)) + 0x17557455;":
                "MemorySnapshotGalaxyAddressToken = reinterpret_cast<std::uintptr_t>(aGalaxy::Galaxy) + 0x17557455;",
            "static_cast<std::uintptr_t>(static_cast<std::uint32_t>(MemorySnapshotGalaxyToken - 0x17557455))":
                "static_cast<std::uintptr_t>(MemorySnapshotGalaxyAddressToken - 0x17557455)",
        }
        for old, new in replacements.items():
            if text.count(old) != 1:
                raise RuntimeError(f"snapshot pointer boundary changed: {old}")
            text = text.replace(old, new, 1)
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
