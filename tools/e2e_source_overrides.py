"""Generate narrow Switch source overrides from the pinned, untouched upstream."""

from __future__ import annotations

from pathlib import Path
import re
from e2e56_cache_overrides import apply as apply_cache_safety
from e2e56_geometry_overrides import apply as apply_geometry_reuse


def _replace_exact(text: str, old: str, new: str, count: int, source_name: str) -> str:
    if text.count(old) != count:
        raise RuntimeError(f"{source_name}: expected {count} occurrences of {old[:60]!r}")
    return text.replace(old, new, count)


def replace_dialog_pointer_payloads(text: str, source_name: str) -> str:
    """Preserve pointers passed through the original int32 dialog callback ABI."""
    prefix = "static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>("
    changed = 0
    offset = 0
    while (start := text.find(prefix, offset)) >= 0:
        expression_start = start + len(prefix)
        depth = 1
        end = expression_start
        while depth:
            if end >= len(text):
                raise RuntimeError(f"unbalanced dialog pointer cast in {source_name}")
            if text[end] == "(":
                depth += 1
            elif text[end] == ")":
                depth -= 1
            end += 1
        if text[end:end + 1] != ")":
            raise RuntimeError(f"dialog pointer cast changed in {source_name}")
        expression = text[expression_start:end - 1]
        replacement = (
            "srhd_awa::platform::e2e5_pointer_payload::Register(this, " +
            expression + ")"
        )
        text = text[:start] + replacement + text[end + 1:]
        offset = start + len(replacement)
        changed += 1
    expected = {"fRuinsTalk.cpp": 18, "fTalk.cpp": 12}[source_name]
    if changed != expected:
        raise RuntimeError(f"{source_name} pointer producers changed: {changed}")

    decode = re.compile(
        r"reinterpret_cast<([^>]+)>\(static_cast<std::uintptr_t>\("
        r"static_cast<std::uint32_t>\((Action|ScriptValue|Sender->UserValue|"
        r"Child->UserValue)\)\)\)"
    )
    def decode_pointer(match: re.Match[str]) -> str:
        if source_name == "fTalk.cpp" and match.group(2) == "Sender->UserValue" and \
                "RequestedMapCenter = " in text[text.rfind("\n", 0, match.start()) + 1:match.start()]:
            return match.group(0)  # ObjectId, not a pointer token.
        return ("srhd_awa::platform::e2e5_pointer_payload::Resolve<" +
                match.group(1) + ">(this, " + match.group(2) + ")")
    text = decode.sub(decode_pointer, text)

    # The decoder above also rewrites this expression; release only the row's
    # choice token after its managed object has been freed.
    new_free = "pas::free(srhd_awa::platform::e2e5_pointer_payload::Resolve<pas::Object*>(this, Child->UserValue));"
    if text.count(new_free) != 1:
        raise RuntimeError(f"{source_name} row cleanup boundary changed")
    text = text.replace(new_free, new_free +
        "\n            srhd_awa::platform::e2e5_pointer_payload::Release(this, Child->UserValue);", 1)
    if source_name == "fRuinsTalk.cpp":
        close = "        ClearChoices();\n        MainPanel->OnClose();"
        if text.count(close) != 1:
            raise RuntimeError("RuinsTalk close boundary changed")
        text = text.replace(close, "        ClearChoices();\n"
            "        srhd_awa::platform::e2e5_pointer_payload::ReleaseOwner(this);\n"
            "        MainPanel->OnClose();", 1)
        callback = "            Choice->Callback(Choice->Value);"
        if text.count(callback) != 1:
            raise RuntimeError("RuinsTalk choice callback boundary changed")
        text = text.replace(callback, callback + '''
#if defined(E2E5_HOST_SDL_WINDOW)
            srhd_awa::platform::e2e_stage::LogE2E3("dialog choice callback complete");
#endif''', 1)
        ready = "            Choices->UpdateScrollRanges();"
        if text.count(ready) != 1:
            raise RuntimeError("RuinsTalk choice-ready boundary changed")
        text = text.replace(ready, ready + '''
#if defined(E2E5_HOST_SDL_WINDOW)
            for (auto* row = Choices->FirstChild; row != nullptr; row = row->NextSibling) {
                if (row->UserValue >= 0) continue;
                fTalk::TfTalkA* choice = nullptr;
                try {
                    choice = srhd_awa::platform::e2e5_pointer_payload::Resolve<fTalk::TfTalkA*>(this, row->UserValue);
                } catch (const std::exception&) {
                    continue;
                }
                if (!pas::assigned(choice->Callback)) continue;
                const auto left = std::max(row->HitTestBounds.Left, Choices->HitTestBounds.Left);
                const auto top = std::max(row->HitTestBounds.Top, Choices->HitTestBounds.Top);
                const auto right = std::min(row->HitTestBounds.Right, Choices->HitTestBounds.Right);
                const auto bottom = std::min(row->HitTestBounds.Bottom, Choices->HitTestBounds.Bottom);
                if (left >= right || top >= bottom) continue;
                char detail[100]{};
                std::snprintf(detail, sizeof(detail), "dialog choice ready x=%d y=%d",
                              left + (right - left) / 2, top + (bottom - top) / 2);
                srhd_awa::platform::e2e_stage::LogE2E3(detail);
                break;
            }
#endif''', 1)
        text = '#if defined(E2E5_HOST_SDL_WINDOW)\n#include "e2e_stage.hpp"\n#include <algorithm>\n#include <cstdio>\n#include <exception>\n#endif\n' + text
    else:
        close = "            ClearChoices(false);\n            Count = "
        if text.count(close) != 1:
            raise RuntimeError("Talk close boundary changed")
        text = text.replace(close, "            ClearChoices(false);\n"
            "            srhd_awa::platform::e2e5_pointer_payload::ReleaseOwner(this);\n"
            "            Count = ", 1)
    return '#include "e2e5_pointer_payload.hpp"\n' + text

if __package__:
    from .e2e4_audio_on_overrides import AUDIO_ON_FUNCTIONS
else:
    from e2e4_audio_on_overrides import AUDIO_ON_FUNCTIONS


FUNCTIONS: dict[str, dict[str, str]] = {
    "GR_GraphBuf.cpp": {
        "void TGraphBufGR::RescaleRGBA_HW(std::uint32_t Width, std::uint32_t Height, std::uint8_t CropToAspect, std::int32_t HorizontalAlign, std::int32_t VerticalAlign)": '''
        // Switch's portable D3D surface path cannot guarantee the original
        // staging/render-target round trip. Use the existing CPU aspect crop
        // and RGBA resampler for these load-time UI backgrounds.
        RescaleWithAspect(Width, Height, CropToAspect,
                          HorizontalAlign, VerticalAlign, 5);''',
    },
    "EC_CacheSound.cpp": {
        "void TCSoundEC::LoadFromConfigBuffer(EC_Buf::TBufEC* SourceBuffer, const pas::WideString& LoadOption)": '''
        (void)LoadOption;
        if (SourceBuffer == nullptr || SourceBuffer->DataSize < 0 ||
            SourceBuffer->Position < 0) {
            GR_Main::RaiseWideMessage(u"WAVE format"_wref.get());
            return;
        }
        const auto wave = srhd_awa::platform::e2e4_wav::Parse(
            SourceBuffer->Data, static_cast<std::size_t>(SourceBuffer->DataSize),
            static_cast<std::size_t>(SourceBuffer->Position));
        if (!wave) {
            GR_Main::RaiseWideMessage(u"WAVE format"_wref.get());
            return;
        }
        void* replacement = EC_Mem::AllocEC(static_cast<std::int32_t>(wave.bytes));
        std::memcpy(replacement,
                    static_cast<const std::uint8_t*>(SourceBuffer->Data) + wave.offset,
                    wave.bytes);
        if (SampleData != nullptr) EC_Mem::FreeEC(SampleData);
        SampleData = replacement;
        SampleDataSize = static_cast<std::uint32_t>(wave.bytes);
        Format.FormatTag = MMSystemSdk::WAVE_FORMAT_PCM;
        Format.Channels = wave.channels;
        Format.SamplesPerSecond = wave.rate;
        Format.BitsPerSample = wave.bits;
        Format.BlockAlign = wave.block_align;
        Format.AverageBytesPerSecond = wave.rate * wave.block_align;
        Format.ExtraSize = 0;
        SourceBuffer->SetPosition(static_cast<std::int32_t>(wave.next_position));'''
    },
    "VorbisFile.cpp": {
        "void LoadVorbisLibrary()": '''
        VorbisLoaded = true;
        GR_Main::AppendLogLineThreadSafe("[AUDIO] native libvorbisfile ready"_a);''',
        "std::uint32_t ReadVorbisSource(void* Buffer, std::uint32_t Size, std::uint32_t Count, void* Source)": '''
        if (Buffer == nullptr || Source == nullptr || Size == 0 || Count == 0 ||
            Size > static_cast<std::uint32_t>(INT32_MAX)) return 0;
        const auto elements = std::min<std::uint32_t>(
            Count, static_cast<std::uint32_t>(INT32_MAX) / Size);
        auto* stream = static_cast<EC_FileStream::TFileStreamEC*>(Source);
        const auto bytes = stream->Read(Buffer, static_cast<std::int32_t>(elements * Size));
        return bytes > 0 ? static_cast<std::uint32_t>(bytes) / Size : 0;''',
        "std::int32_t PAS_STDCALL OpenVorbisStream(TOggWorker* Decoder, DirectSound::TSoundWaveFormat& Format, EC_FileStream::TFileStreamEC*& Stream)": '''
        if (Decoder == nullptr || Stream == nullptr) {
            pas::raise(pas::make_exception<pas::Exception>("Vorbis stream missing"_a));
        }
        pas::critical_enter(pas::load_unaligned<pas::CriticalSection*>(Decoder->Lock));
        pas::ScopeExit unlock = [&]() noexcept {
            pas::critical_leave(pas::load_unaligned<pas::CriticalSection*>(Decoder->Lock));
        };
        CloseVorbisStream(Decoder);
        const auto opened = srhd_awa::platform::e2e4_native_vorbis::Open(
            Stream, &ReadNativeVorbisSource);
        if (!opened) {
            GR_Main::AppendLogLineThreadSafe("[AUDIO] Vorbis open failed"_a);
            pas::raise(pas::make_exception<pas::Exception>("Error open audiofile."_a));
        }
        try {
            StoreNativeVorbis(Decoder, opened.decoder);
        } catch (...) {
            srhd_awa::platform::e2e4_native_vorbis::Close(opened.decoder);
            throw;
        }
        Format.FormatTag = MMSystemSdk::WAVE_FORMAT_PCM;
        Format.Channels = static_cast<std::uint16_t>(opened.channels);
        Format.BitsPerSample = 16;
        Format.ExtraSize = 0;
        Format.SamplesPerSecond = opened.rate;
        Format.BlockAlign = static_cast<std::uint16_t>(opened.channels * 2);
        Format.AverageBytesPerSecond = opened.rate * Format.BlockAlign;
        Decoder->Bitstream = 0;
        GR_Main::AppendLogLineThreadSafe("[AUDIO] music status=decoding"_a);
        return 1;''',
        "std::int32_t PAS_STDCALL ReadVorbisSamples(TOggWorker* Decoder, void* Buffer, std::int32_t& ByteCount)": '''
        const int requested = ByteCount;
        ByteCount = 0;
        if (Decoder == nullptr) return 0;
        pas::critical_enter(pas::load_unaligned<pas::CriticalSection*>(Decoder->Lock));
        pas::ScopeExit unlock = [&]() noexcept {
            pas::critical_leave(pas::load_unaligned<pas::CriticalSection*>(Decoder->Lock));
        };
#if defined(__SWITCH__)
        const auto e2e_decode_begin = std::chrono::steady_clock::now();
#endif
        const auto read = srhd_awa::platform::e2e4_native_vorbis::Read(
            NativeVorbis(Decoder), Buffer, requested);
#if defined(__SWITCH__)
        const auto e2e_decode_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - e2e_decode_begin).count();
        srhd_awa::platform::e2e_stage::audio_decode.Add(e2e_decode_us);
        const auto e2e_decode_ms = e2e_decode_us / 1000;
        static std::atomic<unsigned> e2e_slow_decode_logs{0};
        if (e2e_decode_ms >= 100 && e2e_slow_decode_logs.fetch_add(1) < 8) {
            char detail[112]{};
            std::snprintf(detail, sizeof(detail),
                "music decode slow ms=%lld requested=%d bytes=%d",
                static_cast<long long>(e2e_decode_ms), requested, read.bytes);
            srhd_awa::platform::e2e_stage::Log(detail);
        }
#endif
        if (read.error != 0) {
            GR_Main::AppendLogLineThreadSafe("[MUSIC] Vorbis decode error"_a);
        }
        ByteCount = read.bytes;
        return read.bytes;''',
        "void TOggWorker_Create(TOggWorker* Self, pas::CriticalSection*& SharedLock, std::uint8_t UseExternalLibrary)": '''
        pas::object_create(Self);
        Self->Lock = &SharedLock;
        StoreNativeVorbis(Self, nullptr);
        if (!UseExternalLibrary) {
            pas::critical_enter(pas::load_unaligned<pas::CriticalSection*>(Self->Lock));
            if (!VorbisLoaded) {
                VorbisUseCount = 1;
                VorbisFile::LoadVorbisLibrary();
            } else {
                ++VorbisUseCount;
            }
            pas::critical_leave(pas::load_unaligned<pas::CriticalSection*>(Self->Lock));
        }
        Self->ExternalLibrary = UseExternalLibrary;''',
        "void TOggWorker_Destroy(TOggWorker* Self)": '''
        pas::critical_enter(pas::load_unaligned<pas::CriticalSection*>(Self->Lock));
        CloseVorbisStream(Self);
        if (!Self->ExternalLibrary && VorbisUseCount > 0) --VorbisUseCount;
        pas::critical_leave(pas::load_unaligned<pas::CriticalSection*>(Self->Lock));
        pas::object_destroy(Self);'''
    },
    "GR_Music.cpp": {
        "void TMusicUnit::Clear()": '''
        ImmediateStop = false;
        if (Buffer != nullptr) Buffer->Clear();
        if (StartPlaybackEvent != 0) {
            WindowsImports::CloseHandle(StartPlaybackEvent);
            StartPlaybackEvent = 0u;
        }
        pas::critical_enter(DecodeLock);
        if (Decoder != nullptr) VorbisFile::CloseVorbisStream(Decoder);
        if (Stream != nullptr) {
            pas::free(Stream);
            Stream = nullptr;
        }
        pas::critical_leave(DecodeLock);''',
        "void TMusicUnit_Destroy(TMusicUnit* Self)": '''
        if (Self->IsRunning()) {
            Self->RequestStop();
            if (Self->StartPlaybackEvent != 0) {
                WindowsSdk::SetEvent(Self->StartPlaybackEvent);
            }
            Self->WaitForIdle(WindowsSdk::INFINITE);
        }
        Self->Clear();
        if (Self->Buffer != nullptr) {
            GR_Main::SoundManager->RemoveBuffer(Self->Buffer);
            Self->Buffer = nullptr;
        }
        if (Self->Decoder != nullptr) {
            pas::free(Self->Decoder);
            Self->Decoder = nullptr;
        }
        if (Self->DecodeLock != nullptr) {
            pas::free(Self->DecodeLock);
            Self->DecodeLock = nullptr;
        }
        EC_Thread::TThreadEC_Destroy(Self);'''
    },
    "aGalaxy.cpp": {
        "void XorStateBytes(std::uint8_t* Data, std::uint32_t Count, std::int32_t& Seed)": '''
        for (std::uint32_t i = 0; i < Count; ++i, ++Data) {
            *Data ^= static_cast<std::uint8_t>(aGalaxy::NextStateXorMask(Seed));
        }'''
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
#include <cstdlib>
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
        platform::e2e_stage::LogBootFailed();
        std::_Exit(1);
    } catch (const std::exception& error) {
        const std::string detail = std::string("FAIL stage=ProgramMain exception=") + error.what();
        platform::e2e_stage::Log(detail.c_str());
        platform::win32_compat::LogRuntimeStats();
        platform::e2e_stage::LogBootFailed();
        std::_Exit(1);
    } catch (...) {
        platform::e2e_stage::Log("FAIL stage=ProgramMain unhandled-exception");
        platform::win32_compat::LogRuntimeStats();
        platform::e2e_stage::LogBootFailed();
        std::_Exit(1);
    }
    platform::win32_compat::LogRuntimeStats();
    if (!platform::e2e_stage::FirstFailure().empty()) {
        platform::e2e_stage::LogBootFailed();
        std::_Exit(1);
    }
    if (!platform::e2e_stage::BootReady()) {
        platform::e2e_stage::Log("BOOT RETURNED EARLY");
        return 1;
    }
    return 0;
}
'''


def generated_source(source: Path, destination: Path, build_git: str = "",
                     manifest_stamp: dict | None = None,
                     audio_on: bool = False, e2e5_profile: bool = False,
                     script_width: bool = False) -> Path:
    overrides = dict(FUNCTIONS.get(source.name, {}))
    if audio_on:
        overrides.update(AUDIO_ON_FUNCTIONS.get(source.name, {}))
    if not overrides and source.name not in ('ab_Global.cpp', 'EC_Cache.cpp', 'ab_MainForm.cpp', 'aModsInfo.cpp', 'aMyFunction.cpp') and source.name not in ("EC_Buf.cpp", "EC_Mem.cpp", "Rangers.cpp", "Globals.cpp", "GI_GAI.cpp", "GI_GI.cpp", "GI_MessageLoop.cpp", "GI_GraphButton.cpp", "GR_Main.cpp", "GR_GraphBuf.cpp", "EC_CacheGAI.cpp", "EC_Data.cpp", "EC_FileStream.cpp", "EC_Thread.cpp", "GI_GAIFile.cpp", "aGalaxy.cpp", "aPirate.cpp", "aPlayer.cpp", "aSaveLoad.cpp", "aScript.cpp", "aScriptFun.cpp", "aShip.cpp", "fAbout.cpp", "fCfgSettings.cpp", "fGameSettings.cpp", "fGameSettings2.cpp", "fHangar.cpp", "fIntroduction.cpp", "fLoad.cpp", "fMainForm.cpp", "fSaveManager.cpp", "fShip2.cpp", "fRuinsTalk.cpp", "fTalk.cpp", "program.cpp"):
        return source
    text = PROGRAM_SOURCE if source.name == "program.cpp" else source.read_text(encoding="utf-8")
    if source.name == "EC_Cache.cpp":
        text = apply_cache_safety(text)
    if source.name == "ab_Global.cpp":
        text = apply_geometry_reuse(text)
    if source.name == "fLoad.cpp":
        text = '#include <atomic>\n#include "win32_compat_sync.hpp"\n#include "e2e_clock.hpp"\n#include "e2e5_failure_diagnostics.hpp"\n' + text
        text = _replace_exact(text, '        SetPriority(1);', '''        SetPriority(1);
        // Resource decoding otherwise competes with the UI on its default core
        // at priority 46. Use the same allowed non-UI core as the idle calculator.
        const bool e2e_loader_core = srhd_awa::platform::win32_compat::PreferCalculationCore(ThreadHandle);
        srhd_awa::platform::e2e5_failure_diagnostics::Write(e2e_loader_core ?
            "cache-loader affinity applied=1" : "cache-loader affinity applied=0");''', 1, source.name)
        start = text.index('    void TCacheLoader_Execute(TCacheLoader* Self) {')
        end = text.index('\n    }', start) + len('\n    }')
        body = text[start:end]
        anchor = '            Count = pas::list_count(Self->PendingLoads);'
        body = _replace_exact(body, anchor, anchor + '''
            pas::ScopeExit e2e_pending_cleanup = [&]() noexcept {
                for (std::int32_t j = 0; j < Count; ++j)
                    pas::free(pas::list_at<EC_Cache::TCacheControlEC>(Self->PendingLoads, j));
                pas::free(Self->PendingLoads);
                Self->PendingLoads = nullptr;
            };''', 1, source.name)
        body = _replace_exact(body, '                pas::free(Control);',
            '                pas::list_put(Self->PendingLoads, I, nullptr);\n                pas::free(Control);', 1, source.name)
        body = _replace_exact(body, '            pas::free(Self->PendingLoads);\n            Self->PendingLoads = nullptr;', '', 1, source.name)
        text = text[:start] + body + text[end:]
        start = text.index('                if (([&] {')
        end = text.index(' && First->CacheKey == Second->CacheKey)', start)
        text = text[:start] + '                if ((pas::class_type(First) == pas::class_type(Second))' + text[end:]
        anchor = '                Control = pas::list_at<EC_Cache::TCacheControlEC>(Self->PendingLoads, I);'
        text = _replace_exact(text, anchor, anchor + '''
                char e2e_key[160]{};
                const auto e2e_view = pas::view(Control->CacheKey);
                for (std::size_t j = 0; j < e2e_view.size() && j + 1 < sizeof(e2e_key); ++j)
                    e2e_key[j] = e2e_view[j] < 128 ? static_cast<char>(e2e_view[j]) : '?';
                char e2e_detail[256]{};
                std::snprintf(e2e_detail, sizeof(e2e_detail), "loader begin index=%d total=%d key=%s", I, Count, e2e_key);
                srhd_awa::platform::e2e5_failure_diagnostics::Write(e2e_detail);
                const auto e2e_started = srhd_awa::platform::e2e_clock::Microseconds();''', 1, source.name)
        anchor = '                ++Self->CompletedLoadCount;'
        text = _replace_exact(text, anchor, anchor + '''
                std::snprintf(e2e_detail, sizeof(e2e_detail), "loader end completed=%d total=%d elapsed_us=%llu key=%s",
                    Self->CompletedLoadCount, Count,
                    static_cast<unsigned long long>(srhd_awa::platform::e2e_clock::Microseconds() - e2e_started), e2e_key);
                srhd_awa::platform::e2e5_failure_diagnostics::Write(e2e_detail);''', 1, source.name)
        text = text.replace('++Self->CompletedLoadCount;',
            'std::atomic_ref<std::int32_t>(Self->CompletedLoadCount).fetch_add(1);')
        text = text.replace('Self->CompletedLoadCount, Count,',
            'std::atomic_ref<std::int32_t>(Self->CompletedLoadCount).load(), Count,')
        text = text.replace('        CompletedLoadCount = 0;',
            '        std::atomic_ref<std::int32_t>(CompletedLoadCount).store(0);')
        text = text.replace('Globals::CacheLoader->CompletedLoadCount,',
            'std::atomic_ref<std::int32_t>(Globals::CacheLoader->CompletedLoadCount).load(),')
    if source.name == "aModsInfo.cpp":
        text = '#include <filesystem>\n#include <algorithm>\n#include "e2e_stage.hpp"\n#include "game_path.hpp"\n' + text
        text = _replace_exact(text, 'namespace aModsInfo {', '''namespace aModsInfo {
    bool E2EModFileExists(const pas::AnsiString& name) {
        std::string path(name.c_str());
        std::replace(path.begin(), path.end(), '\\\\', '/');
        if (!path.starts_with("sdmc:/") && !std::filesystem::path(path).is_absolute())
            path = (std::filesystem::current_path() / path).generic_string();
        return SysUtilsImports::FileExists(pas::AnsiString(path.c_str()));
    }
''', 1, source.name)
        start = text.index('    std::uint8_t LoadModInfo(')
        end = text.index('    void InitializeModInfos()', start)
        body = text[start:end].replace('SysUtilsImports::FileExists(', 'E2EModFileExists(')
        body = _replace_exact(body,
            'Block->LoadFromTextFileWithEncodingProbe(pas::literal_pointer(u"ModuleInfo.txt"), false);',
            'Block->LoadFromTextFileWithEncodingProbe(pas::concat_wide({Folder, u"\\\\ModuleInfo.txt"}).pchar(), false);', 1, source.name)
        body = _replace_exact(body, '    void ScanModFolders(pas::WideString Folder, pas::WideString Prefix) {',
            '''    void ScanModFolders(pas::WideString Folder, pas::WideString Prefix) {
        const auto e2e_saved_directory = SysUtilsImports::GetCurrentDir();
        pas::ScopeExit e2e_restore_directory = [&]() noexcept {
            SysUtilsImports::SetCurrentDir(e2e_saved_directory);
        };''', 1, source.name)
        text = text[:start] + body + text[end:]
        text = _replace_exact(text, '''        Folder = static_cast<pas::WideString>(SavedDir);
        Folder = pas::concat_wide({Folder, u"\\\\Mods"});''',
            '''        const auto e2e_mod_root = srhd_awa::platform::game_path::Resolve("Mods");
        Folder = static_cast<pas::WideString>(pas::AnsiString(e2e_mod_root.c_str()));''', 1, source.name)
        marker = '                            Info->Folder = pas::concat_wide({Prefix, FileName});'
        text = _replace_exact(text, marker, marker + '''
                            const pas::AnsiString e2e_name = static_cast<pas::AnsiString>(Info->Name);
                            char detail[256]{};
                            std::snprintf(detail, sizeof(detail), "mods discovered name=%.100s description_chars=%d", e2e_name.c_str(), Info->FullDescription.length());
                            srhd_awa::platform::e2e_stage::Log(detail);''', 1, source.name)
    if source.name == "ab_MainForm.cpp":
        text = '#include <atomic>\n#include "input_platform.hpp"\n#include "e2e56_arcade_steering.hpp"\n#include "e2e5_stage_profiler.hpp"\n' + text
        text = text.replace('CacheLoader->CompletedLoadCount,',
            'std::atomic_ref<std::int32_t>(CacheLoader->CompletedLoadCount).load(),')
        marker = '                    if (ForwardKeyDown) {'
        # This occurrence belongs to the live, unpaused ship control block.
        start = text.index('                ++ab_Global::ArcadeTickCount;')
        end = text.index('                    Stage = 19;', start)
        body = text[start:end]
        body = _replace_exact(body, marker, '''                    const auto e2e_stick = srhd_awa::platform::input_platform::GetFlightStick();
                    const bool e2e_steering = e2e_stick.x || e2e_stick.y;
                    if (e2e_steering) {
                        auto* ship = ab_Ship::PlayerArcadeShip;
                        const auto command = srhd_awa::platform::e2e56_arcade_steering::Steer(
                            ship->GetProjectedHeading(ship->GetProjectedPosition()), e2e_stick.x, e2e_stick.y);
                        ship->SetTurnInput(command.turn);
                        if (command.thrust) ship->StartThrust(); else ship->StopThrust();
                    } else if (ForwardKeyDown) {''', 1, source.name)
        body = _replace_exact(body, '                    if (TurnLeftKeyDown) {',
            '                    if (e2e_steering) {\n                        // Stick has already selected the shortest turn.\n                    } else if (TurnLeftKeyDown) {', 1, source.name)
        text = text[:start] + body + text[end:]
        start = text.index('    void TfAB::TimerTakt(')
        end = text.index('\n    void ', start + 1)
        body = text[start:end]
        for first, last, stage in [(21, 22, 1), (22, 24, 2), (30, 32, 3), (32, 33, 4), (33, 34, 5), (37, None, 6)]:
            a = body.index(f'            Stage = {first};')
            b = body.index(f'            Stage = {last};', a) if last else body.index('        } catch (...) {', a)
            block = body[a:b]
            body = body[:a] + '            {\n                static srhd_awa::platform::e2e5_stage_profiler::Window e2e_ab_phases("arcade-phase");\n' + f'                srhd_awa::platform::e2e5_stage_profiler::Scope e2e_phase(e2e_ab_phases, {stage});\n' + block + '            }\n' + body[b:]
        text = text[:start] + body + text[end:]
        for method, enabled in [('OnOpen', 'true'), ('OnClose', 'false')]:
            marker = f'    void TfAB::{method}() {{'
            text = _replace_exact(text, marker, marker +
                f'\n        srhd_awa::platform::input_platform::SetArcadeControls({enabled});', 1, source.name)
        for marker, stage in [('    void TfAB::DrawFrame() {', 46),
                              ('    void TfAB::TimerTakt(GI_MessageLoop::PCallbackTimerGI Timer, std::int32_t UserData) {', 47)]:
            text = _replace_exact(text, marker, marker +
                f'\n        srhd_awa::platform::e2e5_stage_profiler::Scope e2e_ab_scope(srhd_awa::platform::e2e5_stage_profiler::detail_window, {stage});', 1, source.name)
    if source.name == "GR_Main.cpp":
        for name in ['AppendLogLineThreadSafe', 'AppendLogTextThreadSafe', 'AppendDebugLogLine']:
            start = text.index(f'    void {name}(const pas::AnsiString& Text) {{')
            end = text.index('\n    }', start) + len('\n    }')
            body = text[start:end]
            body = _replace_exact(body, '        pas::critical_enter(SessionLogLock);',
                '''        pas::critical_enter(SessionLogLock);
        pas::ScopeExit e2e_log_unlock = [&]() noexcept { pas::critical_leave(SessionLogLock); };''', 1, source.name)
            body = _replace_exact(body, '\n        pas::critical_leave(SessionLogLock);', '', 1, source.name)
            text = text[:start] + body + text[end:]
    if source.name == "EC_Thread.cpp":
        start = text.index('    void TThreadEC::ProcessRequests() {')
        end = text.index('    void TThreadEC_Execute(', start)
        body = text[start:end]
        finish = '''            pas::critical_enter(Lock);
            {
                pas::ScopeExit cpp_cleanup_2 = [&]() noexcept {
                    pas::critical_leave(Lock);
                };
                WindowsSdk::ResetEvent(RunningEvent);
                WindowsSdk::SetEvent(IdleEvent);
            }'''
        body = _replace_exact(body, finish, '', 1, source.name)
        body = _replace_exact(body, '            } catch (...) {', '''            } catch (...) {
                char detail[256]{};
                std::snprintf(detail, sizeof(detail), "worker exception class=%s handle=%u",
                    typeid(*this).name(), static_cast<unsigned>(ThreadHandle));
                srhd_awa::platform::e2e5_failure_diagnostics::Write(detail);
                srhd_awa::platform::e2e5_failure_diagnostics::Memory("worker-exception");
''', 1, source.name)
        body = _replace_exact(body, '''            try {
                this->virtual_TThreadEC_Execute();''', '''            pas::ScopeExit e2e_finish_job = [&]() noexcept {
                if (std::uncaught_exceptions() > 0)
                    srhd_awa::platform::e2e_threads::MarkCurrentFailed();
                pas::critical_enter(Lock);
                WindowsSdk::ResetEvent(RunningEvent);
                WindowsSdk::SetEvent(IdleEvent);
                pas::critical_leave(Lock);
            };
            try {
                this->virtual_TThreadEC_Execute();''', 1, source.name)
        text = '#include "e2e_threads.hpp"\n#include "e2e5_failure_diagnostics.hpp"\n#include "win32_compat_sync.hpp"\n#include <exception>\n#include <typeinfo>\n#include <cstdio>\n' + text[:start] + body + text[end:]
        start = text.index('    void TThreadEC::Start() {')
        end = text.index('    std::uint8_t TThreadEC::IsRunning()', start)
        body = text[start:end]
        body = _replace_exact(body, '            if (IsRunning()) {', '''            if (!srhd_awa::platform::win32_compat::IsThreadUsable(ThreadHandle))
                pas::raise(pas::make_exception<pas::Exception>("Worker thread unavailable"_a));
            if (IsRunning()) {''', 1, source.name)
        text = text[:start] + body + text[end:]
    if source.name == "EC_Buf.cpp":
        start = text.index("    void TBufEC::LoadFromFileChunk(")
        end = text.index("    void TBufEC::LoadFromFile(", start)
        body = text[start:end]
        body = _replace_exact(body, "        Clear();\n        SourceFile->AcquireReadWriteHandle();", "        const auto e2e_requested_bytes = ByteCount;\n        const auto e2e_initial_position = SourceFile->GetPointer();\n        Clear();\n        SourceFile->AcquireReadWriteHandle();\n        for (int e2e_attempt = 0; e2e_attempt < 2; ++e2e_attempt) {", 1, source.name)
        body = _replace_exact(body, '''        } catch (...) {
            Clear();
        }''', '''        } catch (...) {
            Clear();
            if (e2e_attempt == 0 && srhd_awa::platform::e2e5_memory_recovery::Reclaim("resource-read", e2e_requested_bytes)) {
                try {
                    SourceFile->SetPointer(e2e_initial_position, 0);
                } catch (...) {
                    SourceFile->ReleaseHandle();
                    throw;
                }
                ByteCount = e2e_requested_bytes;
                continue;
            }
            try {
                const pas::AnsiString e2e_path = static_cast<pas::AnsiString>(SourceFile->GetFileName());
                char detail[384]{};
                std::snprintf(detail, sizeof(detail),
                    "FAIL stage=resource-read path=%.240s requested_bytes=%d remaining_bytes=%d",
                    e2e_path.c_str(), e2e_requested_bytes, ByteCount);
                srhd_awa::platform::e2e_stage::Log(detail);
            } catch (...) {
                // Formatting the path may itself allocate after an OOM.
                srhd_awa::platform::e2e_stage::Log("FAIL stage=resource-read diagnostic-unavailable");
            }
            SourceFile->ReleaseHandle();
            throw;
        }''', 1, source.name)
        body = _replace_exact(body, '        SourceFile->ReleaseHandle();\n        return;',
            '        break;\n        }\n        SourceFile->ReleaseHandle();\n        return;', 1, source.name)
        text = '#include "e2e_stage.hpp"\n#include "e2e5_memory_recovery.hpp"\n#include <cstdio>\n' + text[:start] + body + text[end:]
    if source.name == "EC_Mem.cpp":
        for operation in ("AllocEC", "AllocClearEC", "ReAllocREC"):
            start = text.index("    void* " + operation + "(")
            end = text.index("\n    void", start + 1)
            body = text[start:end]
            anchor = '                GR_Main::AppendLogLineThreadSafe("fail"_a);'
            body = _replace_exact(body, anchor, '''                char detail[128]{};
                std::snprintf(detail, sizeof(detail),
                    "allocation failure operation=OP bytes=%d", ByteCount);
                srhd_awa::platform::e2e_stage::Log(detail);
'''.replace("operation=OP", "operation=" + operation) + anchor,
                2 if operation == "ReAllocREC" else 1, source.name)
            text = text[:start] + body + text[end:]
        text = '#include "e2e_stage.hpp"\n#include <cstdio>\n' + text
    if source.name == "fAbout.cpp":
        text = '#include "units/MMSystem.hpp"\n' + text
        text = _replace_exact(text,
            'pas::bind_method<&TfAbout::ScrollCredits>(this), 0);',
            'pas::bind_method<&TfAbout::ScrollCredits>(this), static_cast<std::int32_t>(MMSystem::timeGetTime()));',
            1, source.name)
        text = _replace_exact(text,
            '        CreditsPanel->SetPosition(ClassesImports::Point(0, CreditsPanel->LocalPosition.Y - 1));',
            '''        // Preserve 1 pixel / 20 ms even when rendering delays the callback.
        const auto previous = static_cast<std::uint32_t>(Timer->UserData);
        const auto steps = (MMSystem::timeGetTime() - previous) / 20u;
        Timer->UserData = static_cast<std::int32_t>(previous + steps * 20u);
        if (!steps) return;
        const auto cycle = static_cast<std::uint32_t>(
            ViewportPanel->ClientSize.Y + CreditsPanel->ClientSize.Y);
        if (!cycle) return;
        const auto offset = static_cast<std::uint32_t>(
            ViewportPanel->ClientSize.Y - CreditsPanel->LocalPosition.Y);
        const auto next = (static_cast<std::uint64_t>(offset) + steps) % cycle;
        CreditsPanel->SetPosition(ClassesImports::Point(0,
            ViewportPanel->ClientSize.Y - static_cast<std::int32_t>(next)));''',
            1, source.name)
    if source.name == "EC_FileStream.cpp" and audio_on:
        text = _replace_exact(text,
            "        Self->BlockSize = 4096;",
            "        // Music is the only current user of this buffer size. Larger\n"
            "        // reads avoid yielding after every 4 KiB of SD input.\n"
            "        Self->BlockSize = BufferBytes == 0x000400ff ? 32768 : 4096;",
            1, source.name)
    if source.name == "EC_Data.cpp" and e2e5_profile:
        # The checksum verifier already loads the complete package entry.
        # Reuse that buffer only when the subsequent read wants the whole
        # file and no caller has an open cursor into the shared file object.
        text = _replace_exact(text,
            "    void VerifyResourceFileChecksum(const pas::WideString& FileName) {",
            "    EC_Buf::TBufEC* VerifyResourceFileChecksumRetain(const pas::WideString& FileName) {",
            1, source.name)
        text = _replace_exact(text,
            "                    pas::free(Buf);\n                    break;",
            "                    return Buf;",
            1, source.name)
        text = _replace_exact(text,
            "    }\n\n    void TDataFileEC_Create(TDataFileEC* Self) {",
            """        return nullptr;
    }

    void VerifyResourceFileChecksum(const pas::WideString& FileName) {
        if (auto* verified = VerifyResourceFileChecksumRetain(FileName)) {
            pas::free(verified);
        }
    }

    void TDataFileEC_Create(TDataFileEC* Self) {""",
            1, source.name)
        text = _replace_exact(text,
            "        EC_Data::VerifyResourceFileChecksum(Entry->SharedFileRef->FileRef->FileName);\n        pas::critical_enter(FileLock);",
            '''        EC_Buf::TBufEC* Verified = EC_Data::VerifyResourceFileChecksumRetain(
            Entry->SharedFileRef->FileRef->FileName);
        if (Verified != nullptr) {
            if (Entry->FileOffset == 0 && Entry->ByteCount < 0 &&
                Entry->SharedFileRef->FileRef->OpenDepth == 0) {
                try {
                    Dest->Clear();
                    if (Verified->DataSize > 0) {
                        Dest->AddBytes(Verified->Data, Verified->DataSize);
                    }
                    Dest->SetPosition(0);
                } catch (...) {
                    pas::free(Verified);
                    throw;
                }
                pas::free(Verified);
                return;
            }
            pas::free(Verified);
        }
        pas::critical_enter(FileLock);''',
            1, source.name)
    if source.name == "aPlayer.cpp":
        old_slots = "reinterpret_cast<aShip::PShipEquipmentCacheView>(this)->Slots"
        if text.count(old_slots) != 1:
            raise RuntimeError("player equipment reset boundary changed")
        text = text.replace(old_slots, "Hull", 1)
        player_width_repairs = (
            ("std::uint32_t EncodedPlayer = 0xb1cd15d3u;",
             "std::uintptr_t EncodedPlayer = 0xb1cd15d3u;"),
            ("static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Player)) ^ 0xb1cd15d3u",
             "reinterpret_cast<std::uintptr_t>(Player) ^ 0xb1cd15d3u"),
            ("static_cast<std::uintptr_t>(static_cast<std::uint32_t>(EncodedPlayer ^ 0xb1cd15d3u))",
             "static_cast<std::uintptr_t>(EncodedPlayer ^ 0xb1cd15d3u)"),
        )
        if script_width:
            for old, new in player_width_repairs:
                if text.count(old) != 1:
                    raise RuntimeError(f"encoded player pointer boundary changed: {old}")
                text = text.replace(old, new)
    if source.name == "aShip.cpp":
        # Script state targets already use native-width storage in the
        # disposable script overlay.  Keep that width when turning them back
        # into live game objects during new-game day calculation.
        for field, expected in (("State", 7), ("Place", 3)):
            old = f"static_cast<std::uint32_t>({field}->TargetValue)"
            if text.count(old) != expected:
                raise RuntimeError(f"ship script target boundary changed: {field}")
            text = text.replace(old, f"static_cast<std::uintptr_t>({field}->TargetValue)")
        # The translator's PShipEquipmentCacheView hard-codes the 32-bit
        # offset $F8.  TShip::Hull moves on 64-bit, while Hull..DefGenerator
        # remain eight contiguous pointer fields.  Anchor every indexed slot
        # operation to the actual member without changing upstream/cpp.
        for owner, expected in (("Self", 3), ("this", 12)):
            old = f"reinterpret_cast<PShipEquipmentCacheView>({owner})->Slots"
            if text.count(old) != expected:
                raise RuntimeError(f"ship equipment slot boundary changed: {owner}")
            text = text.replace(old, "Self->Hull" if owner == "Self" else "Hull")
        text = text.replace('#include "layout/aShip.hpp"',
            '#include "layout/aShip.hpp"\n#include <cstddef>\n'
            'static_assert(offsetof(aShip::TShip, DefGenerator) - '
            'offsetof(aShip::TShip, Hull) == 7 * sizeof(void*));', 1)
        tech_begin = text.index("    std::uint8_t TShip_CanUseEquipmentTech(")
        tech_end = text.index("    std::uint8_t TShip_CanRepairEquipmentTech(", tech_begin)
        tech = text[tech_begin:tech_end]
        old = '''        } else if (aTranclucator::TTranclucator* tranclucator = pas::class_cast_if<aTranclucator::TTranclucator*>(Self); tranclucator != nullptr && tranclucator->OwnerShip != nullptr) {
            return aShip::TShip_CanUseEquipmentTech(tranclucator->OwnerShip, Item);
        } else if (pas::class_cast_if<aKling::TKling*>(Self) != nullptr || Item->OwnerId != aGalaxyStruct::oiDominator) {
            return true;
        } else {'''
        new = '''        } else if (Item->OwnerId != aGalaxyStruct::oiDominator) {
            // Ordinary equipment needs no ship-type or knowledge checks.
            return true;
        } else if (aTranclucator::TTranclucator* tranclucator = pas::class_cast_if<aTranclucator::TTranclucator*>(Self); tranclucator != nullptr && tranclucator->OwnerShip != nullptr) {
            return aShip::TShip_CanUseEquipmentTech(tranclucator->OwnerShip, Item);
        } else if (pas::class_cast_if<aKling::TKling*>(Self) != nullptr) {
            return true;
        } else {'''
        if tech.count(old) != 1:
            raise RuntimeError("equipment-tech short-circuit boundary changed")
        text = text[:tech_begin] + tech.replace(old, new, 1) + text[tech_end:]
        bonus_begin = text.index("    std::int32_t TShip::GetTotalStatBonus(")
        bonus_end = text.index("    std::int32_t TShip::GetRadarRange(", bonus_begin)
        bonus = text[bonus_begin:bonus_end]
        # With no micro module, special module or extra specials,
        # TEquipment::GetStatBonus returns zero for every bonus kind.  Avoid
        # ship-type and equipment-tech checks for these common entries.
        inventory_condition = "if ((I == 0 || Item->EquippedFlag != 0) && aShip::TShip_IsEquipmentUsable(this, Item)) {"
        inventory_guard = '''if ((I == 0 || Item->EquippedFlag != 0) && Item != nullptr &&
                (Item->MicroModuleIndex != 0 || Item->SpecialModuleIndex != 0 ||
                 Item->ExtraSpecials != nullptr) &&
                aShip::TShip_IsEquipmentUsable(this, Item)) {'''
        if bonus.count(inventory_condition) != 1:
            raise RuntimeError("inventory stat bonus boundary changed")
        bonus = bonus.replace(inventory_condition, inventory_guard, 1)
        artefact_condition = "if (Item->EquippedFlag != 0 && Item->BrokenFlag == 0) {"
        artefact_guard = '''if (Item->EquippedFlag != 0 && Item->BrokenFlag == 0 &&
                    (Item->MicroModuleIndex != 0 || Item->SpecialModuleIndex != 0 ||
                     Item->ExtraSpecials != nullptr)) {'''
        if bonus.count(artefact_condition) != 1:
            raise RuntimeError("artefact stat bonus boundary changed")
        bonus = bonus.replace(artefact_condition, artefact_guard, 1)
        old = "        std::int32_t Strength = System::Round(GetCombatStatusStrength(cseMagnetic));"
        new = '''        // Magnetic status changes only these four bonuses.  Its lookup is
        // read-only, so skip the list scan for every other bonus kind.
        if (BonusKind != aConst::bonScan && BonusKind != aConst::bonDef &&
            BonusKind != aConst::bonWRadius && BonusKind != aConst::bonRadar) {
            return Result;
        }
        std::int32_t Strength = System::Round(GetCombatStatusStrength(cseMagnetic));'''
        if bonus.count(old) != 1:
            raise RuntimeError("magnetic stat bonus boundary changed")
        text = text[:bonus_begin] + bonus.replace(old, new, 1) + text[bonus_end:]
    if source.name == "aPirate.cpp":
        # Native expression reduced PlanetQueue + 0 to a signed 32-bit
        # integer before treating it as a list pointer.
        old = "reinterpret_cast<pas::List*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(PlanetQueue)) + 0)))"
        if text.count(old) != 1:
            raise RuntimeError("pirate service planet queue boundary changed")
        text = text.replace(old, "PlanetQueue", 1)
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
    if overrides and source.name != "GR_GraphBuf.cpp":
        text = '#include "e2e_clock.hpp"\n' + text
    if audio_on and source.name == "GR_Sound.cpp":
        text = '''#include "e2e4_game_audio_bridge.hpp"
#include "units/SysUtilsImports.hpp"
#include <atomic>
#include <algorithm>
#include <climits>
#include <string>
#include <vector>
''' + text
        helper = '''namespace {
srhd_awa::platform::e2e4_game_audio::Format E2E4AudioFormat(
    const DirectSound::TSoundWaveFormat* wave) {
    return {wave->FormatTag, wave->Channels, wave->SamplesPerSecond,
            wave->AverageBytesPerSecond, wave->BlockAlign, wave->BitsPerSample};
}
}

'''
        marker = "namespace GR_Sound {"
        if text.count(marker) != 1:
            raise RuntimeError("original GR_Sound namespace boundary changed")
        text = text.replace(marker, helper + marker, 1)
    if source.name == "EC_CacheSound.cpp":
        text = '#include "e2e4_wav.hpp"\n#include <cstring>\n' + text
    if source.name == "VorbisFile.cpp":
        native_helpers = '''namespace {
    using NativeDecoder = srhd_awa::platform::e2e4_native_vorbis::Decoder;
    static_assert(sizeof(NativeDecoder*) <= sizeof(TOggWorker::VorbisState));

    NativeDecoder* NativeVorbis(TOggWorker* worker) {
        NativeDecoder* native = nullptr;
        std::memcpy(&native, &worker->VorbisState[0], sizeof(native));
        return native;
    }

    void StoreNativeVorbis(TOggWorker* worker, NativeDecoder* native) {
        std::memcpy(&worker->VorbisState[0], &native, sizeof(native));
    }

    std::size_t ReadNativeVorbisSource(void* output, std::size_t size,
                                      std::size_t count, void* source) {
        if (!output || !source || size == 0 || count == 0 || size > INT32_MAX)
            return 0;
        const std::size_t elements = std::min(count,
            static_cast<std::size_t>(INT32_MAX) / size);
        const std::size_t wanted = elements * size;
        std::size_t total = 0;
        auto* stream = static_cast<EC_FileStream::TFileStreamEC*>(source);
        while (total < wanted) {
#if defined(__SWITCH__)
            const auto e2e_source_begin = std::chrono::steady_clock::now();
#endif
            const auto bytes = stream->Read(static_cast<std::uint8_t*>(output) + total,
                static_cast<std::int32_t>(wanted - total));
#if defined(__SWITCH__)
            const auto e2e_source_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - e2e_source_begin).count();
            srhd_awa::platform::e2e_stage::audio_source.Add(e2e_source_us);
            const auto e2e_source_ms = e2e_source_us / 1000;
            static std::atomic<unsigned> e2e_slow_source_logs{0};
            if (e2e_source_ms >= 100 && e2e_slow_source_logs.fetch_add(1) < 8) {
                char detail[112]{};
                std::snprintf(detail, sizeof(detail),
                    "music source read slow ms=%lld wanted=%zu bytes=%d",
                    static_cast<long long>(e2e_source_ms), wanted - total, bytes);
                srhd_awa::platform::e2e_stage::Log(detail);
            }
#endif
            if (bytes < 0) throw std::runtime_error("Vorbis source read failed");
            if (bytes == 0) break;
            total += static_cast<std::size_t>(bytes);
        }
        return total / size;
    }
}

void CloseVorbisStream(TOggWorker* worker) {
    if (!worker) return;
    auto* native = NativeVorbis(worker);
    StoreNativeVorbis(worker, nullptr);
    srhd_awa::platform::e2e4_native_vorbis::Close(native);
}

'''
        marker = "namespace VorbisFile {\n"
        if text.count(marker) != 1:
            raise RuntimeError("native Vorbis namespace boundary changed")
        text = text.replace(marker, marker + native_helpers, 1)
        text = ('#include "e2e4_native_vorbis.hpp"\n#include "e2e_stage.hpp"\n'
                '#include <algorithm>\n#include <atomic>\n#include <chrono>\n'
                '#include <climits>\n#include <cstdio>\n#include <cstring>\n'
                '#include <stdexcept>\n' + text)
    if source.name == "GR_Music.cpp":
        text = '#include "e2e_threads.hpp"\n' + text
        marker = "namespace GR_Music {\n"
        if text.count(marker) != 1:
            raise RuntimeError("music namespace boundary changed")
        text = text.replace(marker,
            'namespace VorbisFile { void CloseVorbisStream(TOggWorker* worker); }\n'
            + marker, 1)
        music_stream = '''            Stream = pas::construct_call<EC_FileStream::TFileStreamEC>(EC_FileStream::TFileStreamEC_Create, 0x000400ff, FileName);'''
        if text.count(music_stream) != 1:
            raise RuntimeError("music file reader boundary changed")
        text = text.replace(music_stream, music_stream + '''
#if defined(__SWITCH__)
            // Vorbis decoding waits for this separate file-prefetch worker.
            // Its original Lowest priority can starve behind galaxy turns.
            const Result e2e_reader_priority = svcSetThreadPriority(Stream->ThreadId, 0x29);
            srhd_awa::platform::e2e_threads::LogPriority("music-file", Stream->ThreadId, 0x29, e2e_reader_priority);
            if (R_FAILED(e2e_reader_priority)) {
                char detail[96]{};
                std::snprintf(detail, sizeof(detail),
                    "music file reader priority failed result=%u", static_cast<unsigned>(e2e_reader_priority));
                srhd_awa::platform::e2e_stage::Log(detail);
            } else srhd_awa::platform::e2e_stage::Log("music file reader priority=0x29");
#endif''', 1)
        worker = "    void TMusicUnit_Execute(TMusicUnit* Self) {\n"
        if text.count(worker) != 1:
            raise RuntimeError("music decode worker boundary changed")
        text = text.replace(worker, worker + '''#if defined(__SWITCH__)
        // Generation runs its calculation worker at 0x2A. Give the PCM
        // producer a higher priority so that its nine-second ring can refill
        // while that worker is busy; the SDL callback remains independent.
        const Result e2e_music_priority = svcSetThreadPriority(threadGetCurHandle(), 0x29);
            srhd_awa::platform::e2e_threads::LogPriority("music-decode", threadGetCurHandle(), 0x29, e2e_music_priority);
        if (R_FAILED(e2e_music_priority)) {
            char detail[96]{};
            std::snprintf(detail, sizeof(detail),
                "music worker priority failed result=%u", static_cast<unsigned>(e2e_music_priority));
            srhd_awa::platform::e2e_stage::Log(detail);
        } else srhd_awa::platform::e2e_stage::Log("music worker priority=0x29");
#endif
''', 1)
        priority_reset = '''                Self->SetPriority(EC_Thread::ThreadPriorityAboveNormal);'''
        playback_begin = text.index(worker)
        playback_end = text.index("    void TMusicControl_Create(", playback_begin)
        playback = text[playback_begin:playback_end]
        if playback.count(priority_reset) != 1:
            raise RuntimeError("music playback priority reset boundary changed")
        playback = playback.replace(priority_reset, priority_reset + '''
#if defined(__SWITCH__)
                // The original SetPriority above lowers the producer from 0x29
                // to 0x2B after queued playback starts. Restore the priority.
                const Result e2e_playback_priority = svcSetThreadPriority(threadGetCurHandle(), 0x29);
            srhd_awa::platform::e2e_threads::LogPriority("music-playback", threadGetCurHandle(), 0x29, e2e_playback_priority);
                if (R_FAILED(e2e_playback_priority)) {
                    char detail[96]{};
                    std::snprintf(detail, sizeof(detail),
                        "music playback priority failed result=%u", static_cast<unsigned>(e2e_playback_priority));
                    srhd_awa::platform::e2e_stage::Log(detail);
                } else srhd_awa::platform::e2e_stage::Log("music playback priority=0x29");
#endif''', 1)
        text = text[:playback_begin] + playback + text[playback_end:]
        if audio_on:
            control_begin = text.index("    void TMusicControl_Execute(TMusicControl* Self) {")
            control_end = text.index("    void TMusicControl::PlayFile(", control_begin)
            control = text[control_begin:control_end]
            wait = "            WindowsSdk::WaitForSingleObject(Self->CompletionEvent, WindowsSdk::INFINITE);"
            ready = "            if (Self->Queued->IsRunning()) {"
            swap = "                Previous = Self->Current;"
            if control.count(wait) != 1 or control.count(ready) != 1 or control.count(swap) != 1:
                raise RuntimeError("music transition controller boundary changed")
            control = control.replace(wait,
                "            WindowsSdk::WaitForSingleObject(Self->CompletionEvent, 20u);", 1)
            control = control.replace(ready, '''            if (Self->Queued->IsRunning() &&
                srhd_awa::platform::e2e4_game_audio::IsStreamPrimed(Self->Queued->Buffer)) {''', 1)
            control = control.replace(swap, '''                // Keep the old track audible while the new track decodes its
                // initial PCM ring. Request its fade only after priming.
                if (Self->Current->IsRunning()) Self->Current->RequestStop();
''' + swap, 1)
            text = text[:control_begin] + control + text[control_end:]
            play_begin = text.index("    void TMusicControl::PlayFile(")
            play_end = text.index("    void TMusicControl::PlayCategory(", play_begin)
            play = text[play_begin:play_end]
            old_stop = '''            if (Current->IsRunning()) {
                Current->RequestStop();
            } else {
                WindowsSdk::SetEvent(CompletionEvent);
            }'''
            if play.count(old_stop) != 1:
                raise RuntimeError("music transition stop boundary changed")
            play = play.replace(old_stop, '''            if (!Current->IsRunning())
                WindowsSdk::SetEvent(CompletionEvent);''', 1)
            text = text[:play_begin] + play + text[play_end:]
            text = '#include "e2e4_game_audio_bridge.hpp"\n' + text
        text = '#if defined(__SWITCH__)\n#include <switch.h>\n#include <cstdio>\n#include "e2e_stage.hpp"\n#endif\n' + text
    if source.name == "aScript.cpp":
        key = "        std::uint32_t Key = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Block));"
        entry_key = "static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Entry->Block))"
        if text.count(key) != 1 or text.count(entry_key) != 3:
            raise RuntimeError("script UI cache pointer-key boundary changed")
        text = text.replace(key,
            "        const std::uintptr_t Key = srhd_awa::platform::e2e4_ui_cache::Key(Block);", 1)
        text = text.replace(entry_key,
            "srhd_awa::platform::e2e4_ui_cache::Key(Entry->Block)")
        text = '#include "e2e4_ui_cache_key.hpp"\n' + text
    if source.name == "aGalaxy.cpp":
        if e2e5_profile:
            assignment_begin = text.index("    void TGalaxy::AssignTextQuestsToPlanets() {")
            assignment_end = text.index("    std::uint8_t TGalaxy::HasPlayerQuestHistory(", assignment_begin)
            assignment = text[assignment_begin:assignment_end]
            count_anchor = '''        std::int32_t Count = GR_Main::LanguageDataConfig->GetBlockByPath(u"PlanetQuest.PlanetQuest"_wref.get())->GetParamCount();'''
            acquire_anchor = '''                        Buffer = EC_CacheBuf::AcquireOrCreateBuffer(Control);
                        Quest->LoadFromReader(Buffer->Buffer, true);'''
            search_anchor = '''                Index = aMyFunction::NextRandomIntRange(0, pas::list_count(Planets) - 1, RandomState);'''
            free_anchor = '''                pas::free(Quest);'''
            for anchor in (count_anchor, acquire_anchor, search_anchor, free_anchor):
                if assignment.count(anchor) != 1:
                    raise RuntimeError("quest assignment timing boundary changed")
            assignment = assignment.replace(count_anchor, '''        const auto e2e_quest_start = std::chrono::steady_clock::now();
        std::uint64_t e2e_acquire_us = 0, e2e_parse_us = 0, e2e_search_us = 0;
        unsigned e2e_quest_count = 0;
''' + count_anchor, 1)
            assignment = assignment.replace(acquire_anchor, '''                        const auto e2e_acquire_begin = std::chrono::steady_clock::now();
                        Buffer = EC_CacheBuf::AcquireOrCreateBuffer(Control);
                        e2e_acquire_us += std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - e2e_acquire_begin).count();
                        const auto e2e_parse_begin = std::chrono::steady_clock::now();
                        Quest->LoadFromReader(Buffer->Buffer, true);
                        e2e_parse_us += std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - e2e_parse_begin).count();''', 1)
            assignment = assignment.replace(search_anchor,
                '''                const auto e2e_search_begin = std::chrono::steady_clock::now();
''' + search_anchor, 1)
            assignment = assignment.replace(free_anchor, '''                e2e_search_us += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - e2e_search_begin).count();
                ++e2e_quest_count;
''' + free_anchor, 1)
            tail = '''        }
    }

'''
            if not assignment.endswith(tail):
                raise RuntimeError("quest assignment summary boundary changed")
            assignment = assignment[:-len(tail)] + '''        }
        char e2e_quest_detail[180]{};
        std::snprintf(e2e_quest_detail, sizeof(e2e_quest_detail),
            "quest assignment count=%u total_us=%lld acquire_us=%llu parse_us=%llu search_us=%llu",
            e2e_quest_count,
            static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - e2e_quest_start).count()),
            static_cast<unsigned long long>(e2e_acquire_us),
            static_cast<unsigned long long>(e2e_parse_us),
            static_cast<unsigned long long>(e2e_search_us));
        srhd_awa::platform::e2e_stage::LogE2E3(e2e_quest_detail);
    }

'''
            text = text[:assignment_begin] + assignment + text[assignment_end:]
        rating_begin = text.index("    void TGalaxy::RefreshRangerRatingPlaces() {")
        rating_end = text.index("\n    }", rating_begin) + len("\n    }")
        rating = text[rating_begin:rating_end]
        rating_repairs = (
            ("pas::DynArray<std::uint32_t> Sorted{};",
             "pas::DynArray<std::uintptr_t> Sorted{};", 1),
            ("std::uint32_t SwapFirst{};", "std::uintptr_t SwapFirst{};", 1),
            ("std::uint32_t SwapSecond{};", "std::uintptr_t SwapSecond{};", 1),
            ("static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(First))",
             "reinterpret_cast<std::uintptr_t>(First)", 1),
            ("static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Sorted[I]))",
             "Sorted[I]", 2),
            ("static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Sorted[J]))",
             "Sorted[J]", 1),
        )
        for old, new, expected in rating_repairs:
            if rating.count(old) != expected:
                raise RuntimeError(f"ranger rating pointer boundary changed: {old}")
            rating = rating.replace(old, new)
        text = text[:rating_begin] + rating + text[rating_end:]
        text = '#include "e2e4_protected_state.hpp"\n' + text
        array_decl = "        pas::Array<std::uint32_t, 0, 10> Exclusions{};"
        if text.count(array_decl) != 1:
            raise RuntimeError("protected-state exclusion array changed")
        text = text.replace(array_decl,
                            "        pas::Array<std::uintptr_t, 0, 10> Exclusions{};", 1)
        words_begin = "        auto XorStateWords = [&](WindowsSdk::PWORD Data, std::uint32_t Count) -> void {"
        words_end = "        aPlayer::TPlayer* Player = nullptr;"
        if text.count(words_begin) != 1 or text.count(words_end) != 1:
            raise RuntimeError("protected-state object transform boundary changed")
        begin = text.index(words_begin)
        end = text.index(words_end, begin)
        text = text[:begin] + '''        auto XorStateWords = [&](WindowsSdk::PWORD Data, std::uint32_t Count) -> void {
            for (std::uint32_t i = 0; i < Count; ++i, ++Data) {
                pas::store_unaligned<std::uint16_t>(Data,
                    static_cast<std::uint16_t>(pas::load_unaligned<std::uint16_t>(Data) ^
                        static_cast<std::uint16_t>(aGalaxy::NextStateXorMask(Seed))));
            }
        };
        auto XorObject = [&](pas::Object* Instance, const std::uintptr_t* excluded,
                             std::size_t count) -> void {
            const auto bytes = pas::instance_size(pas::class_type(Instance));
            if (!srhd_awa::platform::e2e4_protected_state::TransformObject(
                    Instance, static_cast<std::size_t>(bytes), excluded, count,
                    [&](std::uint8_t* data, std::size_t length) {
                        aGalaxy::XorStateBytes(data, static_cast<std::uint32_t>(length), Seed);
                    })) {
                GR_Main::RaiseWideMessage(u"Invalid protected-state object layout"_wref.get());
            }
        };
        auto XorStateObject = [&](pas::Object* Instance) -> void {
            XorObject(Instance, nullptr, 0);
        };
        auto XorStateObjectExceptField = [&](pas::Object* Instance, void* ExcludedField) -> void {
            const std::uintptr_t excluded = reinterpret_cast<std::uintptr_t>(ExcludedField);
            XorObject(Instance, &excluded, 1);
        };
        auto XorStateObjectExceptFields = [&](pas::Object* Instance) -> void {
            XorObject(Instance, &Exclusions[0], static_cast<std::size_t>(ExclusionCount));
        };
''' + text[end:]
        method_end = text.index("    std::uint32_t NextStateXorMask", begin)
        method = text[begin:method_end]
        method, addresses = re.subn(
            r"static_cast<std::uint32_t>\((reinterpret_cast<std::uintptr_t>\(&[^)]*\))\)",
            r"\1", method)
        if addresses != 13:
            raise RuntimeError(f"protected-state pointer field count changed: {addresses}")
        integrity = ('aGalaxy::XorStateBytes(reinterpret_cast<std::uint8_t*>(&aConst::IntegrityDataBegin), '
                     'reinterpret_cast<std::uintptr_t>(&aConst::IntegrityDataEnd) - '
                     'reinterpret_cast<std::uintptr_t>(&aConst::IntegrityDataBegin), Seed);')
        if method.count(integrity) != 1:
            raise RuntimeError("protected-state integrity range changed")
        method = method.replace(integrity, '''const auto integrity_begin = reinterpret_cast<std::uintptr_t>(&aConst::IntegrityDataBegin);
        const auto integrity_end = reinterpret_cast<std::uintptr_t>(&aConst::IntegrityDataEnd);
        if (integrity_end < integrity_begin ||
            integrity_end - integrity_begin > UINT32_MAX) {
            GR_Main::RaiseWideMessage(u"Invalid protected-state integrity range"_wref.get());
            return;
        }
        aGalaxy::XorStateBytes(reinterpret_cast<std::uint8_t*>(integrity_begin),
            static_cast<std::uint32_t>(integrity_end - integrity_begin), Seed);''', 1)
        text = text[:begin] + method + text[method_end:]
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
        screen_loop = "Globals::RunMainScreenStateLoop();"
        if text.count(screen_loop) != 1:
            raise RuntimeError("main screen loop boundary changed")
        text = text.replace(screen_loop, screen_loop + '''
                                            {
                                                const auto presentation = srhd_awa::platform::renderer_platform::LastPresentationDiagnostics();
                                                char detail[256]{};
                                                std::snprintf(detail, sizeof(detail),
                                                    "screen loop returned requested=%u postload=%u presents=%llu hash=%016llX renderer=%u texture=%u present_ok=%u output=%dx%d",
                                                    static_cast<unsigned>(GlobalsV::RequestedScreenId),
                                                    static_cast<unsigned>(GlobalsV::PostLoadScreenId),
                                                    static_cast<unsigned long long>(srhd_awa::platform::renderer_platform::PresentationCount()),
                                                    static_cast<unsigned long long>(srhd_awa::platform::renderer_platform::LastPresentationHash()),
                                                    static_cast<unsigned>(presentation.renderer_ready),
                                                    static_cast<unsigned>(presentation.texture_ready),
                                                    static_cast<unsigned>(presentation.present_succeeded),
                                                    presentation.output_width, presentation.output_height);
                                                srhd_awa::platform::e2e_stage::Log(detail);
                                            }''', 1)
        text = '#include "renderer_platform.hpp"\n' + text
        text = '#include "e2e_stage.hpp"\n' + text
    if source.name == "Globals.cpp":
        text = '#include "win32_compat_sync.hpp"\n' + text
        text = _replace_exact(text, '        TurnCalculationThread->SetPriority(2);',
            '        TurnCalculationThread->SetPriority(2);\n        srhd_awa::platform::win32_compat::PreferCalculationCore(TurnCalculationThread->ThreadHandle);', 1, source.name)
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
        start = text.index("    void RunMainScreenStateLoop() {")
        end = text.index("\n    }\n", start) + len("\n    }")
        loop = text[start:end]
        dispatch = "GlobalsV::RequestedScreenId = GlobalsV::screenNone;"
        if loop.count(dispatch) != 2:
            raise RuntimeError("screen dispatch boundary changed")
        loop = loop.replace(dispatch, dispatch + '''
                {
                    char detail[96]{};
                    std::snprintf(detail, sizeof(detail), "screen dispatch id=%u postload=%u",
                        static_cast<unsigned>(GlobalsV::CurrentScreenId),
                        static_cast<unsigned>(GlobalsV::PostLoadScreenId));
                    srhd_awa::platform::e2e_stage::Log(detail);
                    if (GlobalsV::PreviousScreenId == GlobalsV::screenMainMenu &&
                        GlobalsV::CurrentScreenId == GlobalsV::screenNewGame)
                        srhd_awa::platform::e2e_stage::LogE2E3("screen_transition from=main_menu to=new_game");
                }''')
        for call in ("->RunContinuous();", "->Run();"):
            if loop.count(call) != 1:
                raise RuntimeError(f"screen return boundary changed: {call}")
            loop = loop.replace(call, call + '''
                {
                    char detail[96]{};
                    std::snprintf(detail, sizeof(detail), "screen returned id=%u requested=%u",
                        static_cast<unsigned>(GlobalsV::CurrentScreenId),
                        static_cast<unsigned>(GlobalsV::RequestedScreenId));
                    srhd_awa::platform::e2e_stage::Log(detail);
                }''', 1)
        text = text[:start] + loop + text[end:]
        text = '#include "e2e_stage.hpp"\n' + text
    if source.name == "GI_MessageLoop.cpp":
        start = text.index("    std::int32_t TMessageLoopGI::Run() {")
        end = text.index("    std::int32_t TMessageLoopGI::RunContinuous() {", start)
        run = text[start:end]
        if run.count("            OnOpen();") != 1 or run.count("                    GR_Main::EndFramePresentation();") != 1:
            raise RuntimeError("screen Run open/presentation boundary changed")
        run = run.replace("            OnOpen();", '''            srhd_awa::platform::e2e_stage::Log("screen Run OnOpen begin");
            OnOpen();
            srhd_awa::platform::e2e_stage::Log("screen Run OnOpen ready");''', 1)
        run = run.replace("                    GR_Main::EndFramePresentation();", '''                    GR_Main::EndFramePresentation();
                    ++e2e_frame_count;
                    if (!e2e_first_frame_logged) {
                        srhd_awa::platform::e2e_stage::Log("screen Run first frame attempted");
                        e2e_first_frame_logged = true;
                    } else if (e2e_frame_count == 2 || e2e_frame_count == 10 ||
                               e2e_frame_count == 60) {
                        char detail[96]{};
                        std::snprintf(detail, sizeof(detail), "screen Run frames=%u",
                            static_cast<unsigned>(e2e_frame_count));
                        srhd_awa::platform::e2e_stage::Log(detail);
                    }''', 1)
        run = run.replace("        std::int32_t Stage = 0;", "        std::int32_t Stage = 0;\n        bool e2e_first_frame_logged = false;\n        std::uint32_t e2e_frame_count = 0;\n        std::uint32_t e2e_draw_window = 0;\n        std::uint64_t e2e_draw_sum = 0;\n        std::uint32_t e2e_draw_max = 0;\n        std::uint32_t e2e_draw_count = 0;", 1)
        draw_start = "                    Stage = 13;\n                    if (GR_Main::OffscreenTexture != nullptr) {"
        draw_end = "                    GR_Main::EndFramePresentation();\n                    ++e2e_frame_count;"
        if run.count(draw_start) != 1 or run.count(draw_end) != 1:
            raise RuntimeError("screen Run draw timing boundary changed")
        run = run.replace(draw_start, "                    Stage = 13;\n                    const auto e2e_draw_start = MMSystem::timeGetTime();\n                    if (GR_Main::OffscreenTexture != nullptr) {", 1)
        run = run.replace(draw_end, '''                    GR_Main::EndFramePresentation();
                    const auto e2e_draw_ms = MMSystem::timeGetTime() - e2e_draw_start;
                    e2e_draw_sum += e2e_draw_ms;
                    if (e2e_draw_ms > e2e_draw_max) e2e_draw_max = e2e_draw_ms;
                    ++e2e_draw_count;
                    if (!e2e_draw_window) e2e_draw_window = MMSystem::timeGetTime();
                    if (MMSystem::timeGetTime() - e2e_draw_window >= 5000u) {
                        char e2e_draw_detail[128]{};
                        std::snprintf(e2e_draw_detail, sizeof(e2e_draw_detail),
                            "screen draw count=%u avg_ms=%llu max_ms=%u",
                            e2e_draw_count,
                            static_cast<unsigned long long>(e2e_draw_count ? e2e_draw_sum / e2e_draw_count : 0),
                            e2e_draw_max);
                        srhd_awa::platform::e2e_stage::Log(e2e_draw_detail);
                        e2e_draw_window = MMSystem::timeGetTime();
                        e2e_draw_sum = 0;
                        e2e_draw_max = 0;
                        e2e_draw_count = 0;
                    }
                    ++e2e_frame_count;''', 1)
        catch_boundary = "        } catch (...) {\n            auto cpp_exception_2 = pas::caught_object();"
        if run.count(catch_boundary) != 1:
            raise RuntimeError("screen Run exception boundary changed")
        run = run.replace(catch_boundary, '''        } catch (...) {
            char e2e_error[192]{};
            try { throw; }
            catch (const std::exception& error) {
                std::snprintf(e2e_error, sizeof(e2e_error),
                    "screen Run exception stage=%d what=%.120s", Stage, error.what());
            }
            catch (...) {
                std::snprintf(e2e_error, sizeof(e2e_error),
                    "screen Run exception stage=%d non-standard", Stage);
            }
            srhd_awa::platform::e2e_stage::Log(e2e_error);
            auto cpp_exception_2 = pas::caught_object();''', 1)
        if e2e5_profile:
            anchor = "        std::int32_t Stage = 0;"
            if run.count(anchor) != 1:
                raise RuntimeError("E2E-5 profiler stage initialization boundary changed")
            run = run.replace(anchor, anchor + "\n        srhd_awa::platform::e2e5_stage_profiler::Window e2e_profile;\n        e2e_profile.Enter(0);", 1)
            run, switches = re.subn(r"(?m)^([ \t]*)Stage = (\d+);$",
                lambda match: f"{match.group(1)}e2e_profile.Enter({match.group(2)});\n{match.group(0)}", run)
            if switches < 20:
                raise RuntimeError(f"E2E-5 profiler stage inventory changed: {switches}")
        ready_anchor = "                    ++e2e_frame_count;"
        if run.count(ready_anchor) != 1:
            raise RuntimeError("main menu ready boundary changed")
        run = run.replace(ready_anchor, '''                    if (GlobalsV::CurrentScreenId == GlobalsV::screenMainMenu &&
                        srhd_awa::platform::renderer_platform::LastPresentationDiagnostics().present_succeeded)
                        srhd_awa::platform::e2e_stage::MarkBootReady();
                    ++e2e_frame_count;''', 1)
        text = text[:start] + run + text[end:]
        continuous_start = text.index("    std::int32_t TMessageLoopGI::RunContinuous() {")
        continuous_end = text.index("    void TMessageLoopGI::AdvanceTimerTick() {", continuous_start)
        continuous = text[continuous_start:continuous_end]
        for anchor in ("            Stage = 1;", "            Stage = 6;\n            OnOpen();",
                       "            if (ExitCode == 0) {\n                DrawFrame();\n            }"):
            if continuous.count(anchor) != 1:
                raise RuntimeError("continuous screen checkpoint boundary changed")
        continuous = continuous.replace("            Stage = 1;",
            '            srhd_awa::platform::e2e_stage::Log("screen RunContinuous begin");\n            Stage = 1;', 1)
        continuous = continuous.replace("        std::int32_t Stage = 0;",
            "        std::int32_t Stage = 0;\n        std::uint32_t e2e_continuous_frames = 0;", 1)
        continuous = continuous.replace("            Stage = 6;\n            OnOpen();", '''            Stage = 6;
            srhd_awa::platform::e2e_stage::Log("screen RunContinuous OnOpen begin");
            OnOpen();
            srhd_awa::platform::e2e_stage::Log("screen RunContinuous OnOpen ready");''', 1)
        continuous = continuous.replace("            if (ExitCode == 0) {\n                DrawFrame();\n            }", '''            if (ExitCode == 0) {
                DrawFrame();
                ++e2e_continuous_frames;
                char e2e_frame[128]{};
                std::snprintf(e2e_frame, sizeof(e2e_frame),
                    "screen RunContinuous frames=%u presentations=%llu",
                    e2e_continuous_frames,
                    static_cast<unsigned long long>(
                        srhd_awa::platform::renderer_platform::PresentationCount()));
                srhd_awa::platform::e2e_stage::Log(e2e_frame);
            }''', 1)
        loop_drawn = "                    Stage = 19;\n                    SysUtilsImports::Sleep(1u);"
        if continuous.count(loop_drawn) != 1:
            raise RuntimeError("continuous screen frame milestone boundary changed")
        continuous = continuous.replace(loop_drawn, '''                    Stage = 19;
                    ++e2e_continuous_frames;
                    if (e2e_continuous_frames == 2 || e2e_continuous_frames == 10 ||
                        e2e_continuous_frames == 60) {
                        char e2e_frame[128]{};
                        std::snprintf(e2e_frame, sizeof(e2e_frame),
                            "screen RunContinuous frames=%u presentations=%llu",
                            e2e_continuous_frames,
                            static_cast<unsigned long long>(
                                srhd_awa::platform::renderer_platform::PresentationCount()));
                        srhd_awa::platform::e2e_stage::Log(e2e_frame);
                    }
                    SysUtilsImports::Sleep(1u);''', 1)
        text = text[:continuous_start] + continuous + text[continuous_end:]
        if e2e5_profile:
            for entry in ("    std::int32_t TMessageLoopGI::Run() {",
                          "    std::int32_t TMessageLoopGI::RunContinuous() {"):
                text = _replace_exact(text, entry, entry +
                    "\n        srhd_awa::platform::e2e5_stage_profiler::SetScreen(GlobalsV::CurrentScreenId);",
                    1, source.name)
            object_draw_start = text.index("    void TObjectGI::Draw(WindowsSdk::TRect ClipRect) {")
            object_draw_end = text.index("    void TObjectGI::DrawUpdateRects(", object_draw_start)
            object_draw = text[object_draw_start:object_draw_end]
            object_draw = _replace_exact(object_draw,
                "                Child->Draw(Intersection);",
                '''                const auto e2e_child_start = std::chrono::steady_clock::now();
                Child->Draw(Intersection);
                const auto e2e_child_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - e2e_child_start).count();
                static std::int32_t e2e_slow_child_screen = -1;
                static std::uint32_t e2e_slow_child_logs = 0;
                if (e2e_slow_child_screen != GlobalsV::CurrentScreenId) {
                    e2e_slow_child_screen = GlobalsV::CurrentScreenId;
                    e2e_slow_child_logs = 0;
                }
#if defined(E2E5_HOST_SDL_WINDOW)
                constexpr auto e2e_slow_child_threshold_us = 500;
#else
                constexpr auto e2e_slow_child_threshold_us = 50000;
#endif
                if (e2e_child_us >= e2e_slow_child_threshold_us && e2e_slow_child_logs < 8) {
                    const pas::AnsiString e2e_child_name = static_cast<pas::AnsiString>(Child->ControlName);
                    const pas::AnsiString e2e_child_type = pas::class_name(pas::class_type(Child));
                    char e2e_detail[240]{};
                    std::snprintf(e2e_detail, sizeof(e2e_detail),
                        "ui child draw screen=%d type=%.60s name=%.60s us=%lld bounds=%d,%d,%d,%d",
                        GlobalsV::CurrentScreenId, e2e_child_type.c_str(),
                        e2e_child_name.c_str(), static_cast<long long>(e2e_child_us),
                        Intersection.Left, Intersection.Top, Intersection.Right, Intersection.Bottom);
                    srhd_awa::platform::e2e_stage::Log(e2e_detail);
                    ++e2e_slow_child_logs;
                }''', 1, source.name)
            text = text[:object_draw_start] + object_draw + text[object_draw_end:]
            draw_updates_start = text.index("    void TObjectGI::DrawUpdateRects(WindowsSdk::TRect ClipRect) {")
            draw_updates_end = text.index("    void TObjectGI::", draw_updates_start + 1)
            draw_updates = text[draw_updates_start:draw_updates_end]
            draw_updates = _replace_exact(draw_updates,
                "    void TObjectGI::DrawUpdateRects(WindowsSdk::TRect ClipRect) {",
                "    void TObjectGI::DrawUpdateRects(WindowsSdk::TRect ClipRect) {\n"
                "        ++srhd_awa::platform::e2e5_stage_profiler::ui_work.control_visits;",
                1, source.name)
            text = text[:draw_updates_start] + draw_updates + text[draw_updates_end:]
            draw_start = text.index("    void TMessageLoopGI::DrawQueuedUpdateRects() {")
            draw_end = text.index("    void TMessageLoopGI::DrawQueuedControlRects() {", draw_start)
            draw = text[draw_start:draw_end]
            draw = _replace_exact(draw,
                "    void TMessageLoopGI::DrawQueuedUpdateRects() {",
                "    void TMessageLoopGI::DrawQueuedUpdateRects() {\n"
                "        static srhd_awa::platform::e2e5_stage_profiler::Window e2e_dirty_profile{\"dirty\"};\n"
                "        e2e_dirty_profile.Enter(30);", 1, source.name)
            draw = _replace_exact(draw,
                "        GR_Rect::TRectGR* RectNode = UpdateRects->FirstRect;",
                "        e2e_dirty_profile.Enter(31);\n"
                "        GR_Rect::TRectGR* RectNode = UpdateRects->FirstRect;", 1, source.name)
            draw = _replace_exact(draw,
                "            RootUiObject->Draw(RectNode->Bounds);",
                "            srhd_awa::platform::e2e5_stage_profiler::QueueRect(\n"
                "                RectNode->Bounds.Left, RectNode->Bounds.Top,\n"
                "                RectNode->Bounds.Right, RectNode->Bounds.Bottom);\n"
                "            ++srhd_awa::platform::e2e5_stage_profiler::ui_work.root_draw_calls;\n"
                "            RootUiObject->Draw(RectNode->Bounds);", 1, source.name)
            draw = _replace_exact(draw,
                "            RectNode = RectNode->Next;\n        }\n    }",
                "            RectNode = RectNode->Next;\n        }\n"
                "        e2e_dirty_profile.Stop();\n    }", 1, source.name)
            text = text[:draw_start] + draw + text[draw_end:]
        draw_start = text.index("    void TMessageLoopGI::DrawQueuedUpdateRects() {")
        draw_end = text.index("    void TMessageLoopGI::DrawQueuedControlRects() {", draw_start)
        draw = text[draw_start:draw_end]
        node = "        GR_Rect::TRectGR* RectNode = UpdateRects->FirstRect;"
        loop = "        while (RectNode != nullptr) {"
        advance = "            RectNode = RectNode->Next;"
        if draw.count(node) != 1 or draw.count(loop) != 1 or draw.count(advance) != 1:
            raise RuntimeError("queued UI rectangle consumption boundary changed")
        draw = draw.replace(node, '''        // Consume the queued coverage before drawing. New invalidations made by
        // a control during Draw belong to the next frame.
        std::vector<WindowsSdk::TRect> e2e_pending_rects;
        for (auto* e2e_node = UpdateRects->FirstRect; e2e_node != nullptr;
             e2e_node = e2e_node->Next)
            e2e_pending_rects.push_back(e2e_node->Bounds);
        UpdateRects->Clear();''', 1)
        draw = draw.replace(loop, "        for (const auto& e2e_rect : e2e_pending_rects) {", 1)
        draw = draw.replace("RectNode->Bounds", "e2e_rect")
        draw = draw.replace(advance, "", 1)
        text = text[:draw_start] + draw + text[draw_end:]
        text = ('#include "e2e5_stage_profiler.hpp"\n' if e2e5_profile else '') + '#include "renderer_platform.hpp"\n#include "e2e_stage.hpp"\n#include <cstdio>\n#include <vector>\n' + text
    if source.name == "GI_GraphButton.cpp":
        timer_gate = "pas::assigned(UpCallback) && MessageLoop->ConsumeTimerTickChange()"
        if text.count(timer_gate) != 1:
            raise RuntimeError("original graph-button release timer gate changed")
        # A valid down/up edge is an action even when two polls share one UI tick.
        # The input bridge emits one release per physical A edge.
        text = text.replace(timer_gate,
            "pas::assigned(UpCallback) && (MessageLoop->ConsumeTimerTickChange() || WasDown)", 1)
        for event, signature in (
            ("down", "    void TGraphButtonGI::ProcessLeftButtonDown("),
            ("up", "    void TGraphButtonGI::ProcessLeftButtonUp("),
        ):
            begin = text.index(signature)
            end = text.index("\n    void TGraphButtonGI::", begin + len(signature))
            original = text[begin:end]
            base = f"GI_MessageLoop::TObjectGI::ProcessLeftButton{event.title()}(KeyState, Point);"
            if original.count(base) != 1:
                raise RuntimeError(f"graph-button {event} diagnostic boundary changed")
            detail = f'''
#if defined(E2E3_TRACE) && E2E3_TRACE
        if (pas::view(ControlName) == u"New"sv) {{
            char detail[224]{{}};
            std::snprintf(detail, sizeof(detail),
                "button=New press_id=%llu event={event} x=%d y=%d hit=%u hovered=%u focused=%u occluded=%u disabled=%u down=%u kind=%u callback=%u",
                static_cast<unsigned long long>(srhd_awa::platform::win32_compat::CurrentPressId()),
                Point.X, Point.Y, static_cast<unsigned>(HitTest(Point)),
                static_cast<unsigned>(MessageLoop->HoveredControl == this),
                static_cast<unsigned>(MessageLoop->FocusedControl == this),
                static_cast<unsigned>(IsOccludedAtPoint(Point)),
                static_cast<unsigned>(Disabled), static_cast<unsigned>(Down),
                static_cast<unsigned>(Kind), static_cast<unsigned>(pas::assigned(UpCallback)));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }}
#endif'''
            text = text[:begin] + original.replace(base, base + detail, 1) + text[end:]
        text = '#include "e2e_stage.hpp"\n#include "win32_compat_messages.hpp"\n#include <cstdio>\n' + text
    if source.name == "fMainForm.cpp":
        open_method = "    void TfMainForm::OnOpen() {"
        callback = "    void TfMainForm::NewGameClicked(GI_MessageLoop::TObjectGI* Sender) {"
        request = "        GlobalsV::RequestedScreenId = GlobalsV::screenNewGame;"
        keyboard = "            NewGameClicked(nullptr);"
        if any(text.count(value) != 1 for value in (open_method, callback, request, keyboard)):
            raise RuntimeError("original main menu new-game boundary changed")
        text = text.replace(open_method, open_method +
            '\n        srhd_awa::platform::e2e_stage::LogE2E3("main_menu status=open");', 1)
        new_binding = '        pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(GetByName(u"New"sv))->UpCallback = pas::bind_method<&TfMainForm::NewGameClicked>(this);'
        if text.count(new_binding) != 1:
            raise RuntimeError("original New button binding boundary changed")
        text = text.replace(new_binding, new_binding + '''
#if defined(E2E3_TRACE) && E2E3_TRACE
        {
            const auto* button = GetByName(u"New"sv);
            char detail[128]{};
            std::snprintf(detail, sizeof(detail),
                "menu_new_bounds left=%d top=%d right=%d bottom=%d active=%u",
                button->HitTestBounds.Left, button->HitTestBounds.Top,
                button->HitTestBounds.Right, button->HitTestBounds.Bottom,
                static_cast<unsigned>(button->Active));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }
#endif''', 1)
        load_binding = '        pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(GetByName(u"Load"sv))->UpCallback = pas::bind_method<&TfMainForm::LoadGameClicked>(this);'
        if text.count(load_binding) != 1:
            raise RuntimeError("original Load button binding boundary changed")
        text = text.replace(load_binding, load_binding + '''
#if defined(E2E3_TRACE) && E2E3_TRACE
        {
            const auto* button = GetByName(u"Load"sv);
            char detail[128]{};
            std::snprintf(detail, sizeof(detail),
                "menu_load_bounds left=%d top=%d right=%d bottom=%d active=%u",
                button->HitTestBounds.Left, button->HitTestBounds.Top,
                button->HitTestBounds.Right, button->HitTestBounds.Bottom,
                static_cast<unsigned>(button->Active));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }
#endif''', 1)
        text = text.replace(request, request + '''
        {
            char detail[120]{};
            std::snprintf(detail, sizeof(detail),
                "menu_action=New press_id=%llu result=accepted requested=new_game",
                static_cast<unsigned long long>(srhd_awa::platform::win32_compat::CurrentPressId()));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }''', 1)
        text = text.replace(keyboard,
            '            srhd_awa::platform::e2e_stage::LogE2E3("menu_action=New source=keyboard key=Enter");\n' + keyboard, 1)
        text = '#include "e2e_stage.hpp"\n#include "win32_compat_messages.hpp"\n#include <cstdio>\n' + text
    if source.name == "fSaveManager.cpp":
        # The original uses both Save and save for one directory. The portable
        # host filesystem resolves those as different paths, while the slot
        # writer and user-root layout already use the lowercase spelling.
        directory = 'GR_Main::GetGameUserDirectory(), u"Save"'
        if text.count(directory) != 2:
            raise RuntimeError("original SaveManager directory boundary changed")
        text = text.replace(directory, 'GR_Main::GetGameUserDirectory(), u"save"')
        scan_call = '        ScanSaveFiles();'
        if text.count(scan_call) != 1:
            raise RuntimeError("original SaveManager slot scan boundary changed")
        text = text.replace(scan_call, scan_call + '''
        {
            char detail[96]{};
            std::snprintf(detail, sizeof(detail), "save_scan slots=%d newest=%d",
                          pas::list_count(Slots), FindNewestSlot());
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }''', 1)
        load_callback = '    void TfSaveManager::LoadClicked(GI_MessageLoop::TObjectGI* Sender) {'
        if text.count(load_callback) != 1:
            raise RuntimeError("original SaveManager load callback boundary changed")
        text = text.replace(load_callback, load_callback + '''
        {
            char detail[96]{};
            std::snprintf(detail, sizeof(detail), "save_load_clicked selected=%d slots=%d",
                          SelectedSlot, pas::list_count(Slots));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }''', 1)
        scan_rejection = '''                            } catch (...) {
                                pas::dispose(Slot);
                            }'''
        if text.count(scan_rejection) != 1:
            raise RuntimeError("original SaveManager scan rejection boundary changed")
        text = text.replace(scan_rejection, '''                            } catch (...) {
                                srhd_awa::platform::e2e_stage::LogE2E3("save_scan slot_rejected");
                                pas::dispose(Slot);
                            }''', 1)
        scan_search = '                Search = WindowsSdk::FindFirstFile(pas::literal_pointer("*.sav"), FindData);'
        if text.count(scan_search) != 1:
            raise RuntimeError("original SaveManager search boundary changed")
        text = text.replace(scan_search, scan_search + '''
                srhd_awa::platform::e2e_stage::LogE2E3(
                    Search == WindowsImports::INVALID_HANDLE_VALUE ?
                    "save_scan search_empty" : "save_scan search_found");''', 1)
        load_binding = '            ButLoad->UpCallback = pas::bind_method<&TfSaveManager::LoadClicked>(this);'
        if text.count(load_binding) != 1:
            raise RuntimeError("original SaveManager Load button binding boundary changed")
        text = text.replace(load_binding, load_binding + '''
#if defined(E2E3_TRACE) && E2E3_TRACE
            {
                char detail[160]{};
                std::snprintf(detail, sizeof(detail),
                    "save_load_bounds left=%d top=%d right=%d bottom=%d active=%u",
                    ButLoad->HitTestBounds.Left, ButLoad->HitTestBounds.Top,
                    ButLoad->HitTestBounds.Right, ButLoad->HitTestBounds.Bottom,
                    static_cast<unsigned>(ButLoad->Active));
                srhd_awa::platform::e2e_stage::LogE2E3(detail);
            }
#endif''', 1)
        text = '#include "e2e_stage.hpp"\n#include <cstdio>\n' + text
    if source.name == "fShip2.cpp":
        ship_run = '        std::uint8_t Result = Globals::ShipScreen->Run() == 1;'
        if text.count(ship_run) != 1:
            raise RuntimeError("original ship equipment modal boundary changed")
        text = text.replace(ship_run,
            '        srhd_awa::platform::e2e_stage::LogE2E3("ship_equipment_open");\n' +
            ship_run + '\n' +
            '        srhd_awa::platform::e2e_stage::LogE2E3("ship_equipment_closed");', 1)
        text = '#include "e2e_stage.hpp"\n' + text
    if source.name == "EC_CacheGAI.cpp":
        frame_start = "    GR_gi::TgiGR* TCGaiEC::LoadFrameGi(std::int32_t FrameIndex) {"
        if text.count(frame_start) != 1:
            raise RuntimeError("GAI frame decoder boundary changed")
        text = text.replace(frame_start, frame_start + '''
        const auto e2e_frame_begin_us = srhd_awa::platform::e2e_clock::Microseconds();
''', 1)
        if e2e5_profile:
            text = '#include "e2e5_stage_profiler.hpp"\n' + text
            text = text.replace(frame_start, frame_start +
                '\n        srhd_awa::platform::e2e5_stage_profiler::Scope e2e_decode(srhd_awa::platform::e2e5_stage_profiler::detail_window, 38);', 1)
        frame_end = '''        if (!DecodedFrameGi->IsEmpty()) {
            return DecodedFrameGi;
        }
        return Result;
    }'''
        if text.count(frame_end) != 1:
            raise RuntimeError("GAI frame decoder return boundary changed")
        text = text.replace(frame_end, '''        const bool e2e_boot_ready = srhd_awa::platform::e2e_stage::BootReady();
        static std::uint32_t e2e_menu_frame_logs = 0;
        static std::uint32_t e2e_intro_frame_logs = 0;
        const bool e2e_menu_frame = e2e_boot_ready &&
            e2e_menu_frame_logs < 48 && CacheKey == u"Bm.FormMain3.2ShipA3";
        bool e2e_intro_frame = false;
        if (!e2e_boot_ready && e2e_intro_frame_logs < 24) {
            const pas::AnsiString e2e_name = static_cast<pas::AnsiString>(CacheKey);
            e2e_intro_frame = std::strstr(e2e_name.c_str(), "FormLoad2") != nullptr;
        }
        if (e2e_menu_frame || e2e_intro_frame) {
            const pas::AnsiString e2e_name = static_cast<pas::AnsiString>(CacheKey);
            char e2e_detail[256]{};
            std::snprintf(e2e_detail, sizeof(e2e_detail),
                "gai %sframe key=%.140s index=%d load_us=%llu bytes=%d format=%d",
                e2e_intro_frame ? "intro " : "menu ",
                e2e_name.c_str(), FrameIndex,
                static_cast<unsigned long long>(srhd_awa::platform::e2e_clock::Microseconds() - e2e_frame_begin_us),
                DecodedFrameGi->DataSize,
                DecodedFrameGi->Header ? DecodedFrameGi->Header->Format : -1);
            srhd_awa::platform::e2e_stage::Log(e2e_detail);
            if (e2e_intro_frame) ++e2e_intro_frame_logs;
            else ++e2e_menu_frame_logs;
        }
        if (!DecodedFrameGi->IsEmpty()) {
            return DecodedFrameGi;
        }
        return Result;
    }''', 1)
        old = '''        TCGaiEC::ApplyAB2BackgroundFixup(SourceBuffer, CacheKey);
        try {
            RawGaiData = EC_Mem::AllocEC(SourceBuffer->DataSize);
            Windows::CopyMemory(RawGaiData, SourceBuffer->Data, SourceBuffer->DataSize);
            ResidentBytes = SourceBuffer->DataSize;
        } catch (...) {
            RawGaiData = nullptr;
            ResidentBytes = 0;
        }
        Header = static_cast<GR_gi::PGaiHeader>(RawGaiData);
        if (SourceBuffer->DataSize < static_cast<std::int32_t>(sizeof(GR_gi::TGaiHeader)) || RawGaiData == nullptr) {
            GR_Main::AppendLogLineThreadSafe("Error Load Gai"_a);
            Header = static_cast<GR_gi::PGaiHeader>(EC_Mem::AllocClearEC(static_cast<std::int32_t>(sizeof(GR_gi::TGaiHeader))));
        }'''
        if text.count(old) != 1:
            raise RuntimeError("GAI loading boundary changed")
        new = '''        if (SourceBuffer == nullptr) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=gai source-buffer-null");
            pas::raise(pas::make_exception<pas::Exception>("GAI source buffer missing"_a));
        }
        TCGaiEC::ApplyAB2BackgroundFixup(SourceBuffer, CacheKey);
        const pas::AnsiString e2e_gai_name = static_cast<pas::AnsiString>(CacheKey);
        char e2e_gai_detail[320]{};
        const bool e2e_terminal_blank = CacheKey == u"Bm.Hits.05" &&
            SourceBuffer->DataSize == 31020;
        const char* e2e_gai_error = SourceBuffer->DataSize < 0 ? "negative size" :
            srhd_awa::platform::e2e5_gai_bounds::Validate(
                SourceBuffer->Data, static_cast<std::size_t>(SourceBuffer->DataSize),
                e2e_terminal_blank);
        if (e2e_gai_error != nullptr) {
            std::snprintf(e2e_gai_detail, sizeof(e2e_gai_detail),
                "FAIL stage=gai key=%s bytes=%d reason=%s",
                e2e_gai_name.c_str(), SourceBuffer->DataSize, e2e_gai_error);
            srhd_awa::platform::e2e_stage::Log(e2e_gai_detail);
            pas::raise(pas::make_exception<pas::Exception>("GAI bounds invalid"_a));
        }
        if (e2e_terminal_blank)
            srhd_awa::platform::e2e_stage::Log("gai terminal blank allowed key=Bm.Hits.05");
        std::snprintf(e2e_gai_detail, sizeof(e2e_gai_detail),
            "gai load key=%s bytes=%d", e2e_gai_name.c_str(), SourceBuffer->DataSize);
        srhd_awa::platform::e2e_stage::Log(e2e_gai_detail);
        RawGaiData = EC_Mem::AllocEC(SourceBuffer->DataSize);
        Windows::CopyMemory(RawGaiData, SourceBuffer->Data, SourceBuffer->DataSize);
        ResidentBytes = SourceBuffer->DataSize;
        Header = static_cast<GR_gi::PGaiHeader>(RawGaiData);'''
        text = text.replace(old, new, 1)
        text = '#include "e2e5_gai_bounds.hpp"\n#include "e2e_stage.hpp"\n#include "e2e_clock.hpp"\n#include <cstdio>\n#include <cstring>\n' + text
    if source.name == "GI_GAIFile.cpp":
        # FrameBuffers is allocated as an array of void*. The original
        # translation used 32-bit integer accessors for its pointer entries.
        slot = "srhd_awa::platform::e2e5_gai_slots::At"
        for index, count in (("I", 2), ("FrameIndex", 2)):
            old = ("reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>("
                   "EC_Mem::ReadDWordEC(EC_Mem::AddPointerOffset(FrameBuffers, " + index +
                   " * static_cast<std::int32_t>(sizeof(void*)))))))")
            if text.count(old) != count:
                raise RuntimeError("GAI file pointer read boundary changed: " + index)
            text = text.replace(old, slot + "(FrameBuffers, " + index + ")", count)
        reads = (
            ("Index", " > 0", " != nullptr"),
            ("Self->Owner->GetSequenceFrame(Index)", " == 0", " == nullptr"),
        )
        for index, old_test, new_test in reads:
            old = ("EC_Mem::ReadDWordEC(EC_Mem::AddPointerOffset(Self->Owner->FrameBuffers, " +
                   index + " * static_cast<std::int32_t>(sizeof(void*))))" + old_test)
            if text.count(old) != 1:
                raise RuntimeError("GAI file preload pointer boundary changed: " + index)
            text = text.replace(old, slot + "(Self->Owner->FrameBuffers, " + index + ")" + new_test, 1)
        writes = (
            ("Self->Owner->FrameBuffers", "SourceFrame", "static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Data))", "Data"),
            ("FrameBuffers", "FrameIndex", "static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Data))", "Data"),
            ("FrameBuffers", "I", "0", "nullptr"),
        )
        for buffers, index, old_value, new_value in writes:
            old = ("EC_Mem::WriteIntegerEC(EC_Mem::AddPointerOffset(" + buffers + ", " +
                   index + " * static_cast<std::int32_t>(sizeof(void*))), " + old_value + ");")
            if text.count(old) != 1:
                raise RuntimeError("GAI file pointer write boundary changed: " + index)
            text = text.replace(old, slot + "(" + buffers + ", " + index + ") = " + new_value + ";", 1)
        text = '#include "e2e5_gai_slots.hpp"\n' + text
    if source.name == "GI_GAI.cpp":
        if e2e5_profile:
            text = '#include "e2e5_stage_profiler.hpp"\n' + text
            entry = '    void TgaiGI::Draw(WindowsSdk::TRect ClipRect) {'
            text = _replace_exact(text, entry, entry + '''
        const bool e2e_intro = ControlName == u"Intro" && !FirstFrameOnly && SequenceFrameCount > 1;
        srhd_awa::platform::e2e5_stage_profiler::Scope e2e_composite(
            srhd_awa::platform::e2e5_stage_profiler::detail_window, 46, e2e_intro);
        if (e2e_intro) {
            ++srhd_awa::platform::e2e5_stage_profiler::ui_work.intro_draws;
            srhd_awa::platform::e2e5_stage_profiler::intro_position = SequenceFrame;
            srhd_awa::platform::e2e5_stage_profiler::intro_pending = true;
        }''', 1, source.name)
            start = text.index(entry)
            end = text.index('\n    void TgaiGI::', start + len(entry))
            draw = text[start:end]
            load = 'Frame = Image->LoadFrameGi(GetSequenceFrameSourceIndex(FrameIndex));'
            if draw.count(load) != 3:
                raise RuntimeError('GAI playback composition boundaries changed')
            draw = draw.replace(load, load +
                '\n                            if (e2e_intro) ++srhd_awa::platform::e2e5_stage_profiler::ui_work.intro_applied;')
            text = text[:start] + draw + text[end:]
        tiled_frame = '''                            Frame = Image->LoadFrameGi(GetSequenceFrameSourceIndex(SequenceFrame));
                            Frame->DrawToGraphBuf(Buffer, X + Frame->GetBoundsRect().Left - Bounds.Left, Y + Frame->GetBoundsRect().Top - Bounds.Top, Clip, 0, 255);'''
        if text.count(tiled_frame) != 1:
            raise RuntimeError("GAI tiled frame draw boundary changed")
        text = text.replace(tiled_frame, '''                            Frame = Image->LoadFrameGi(GetSequenceFrameSourceIndex(SequenceFrame));
                            if (Frame != nullptr)
                                Frame->DrawToGraphBuf(Buffer, X + Frame->GetBoundsRect().Left - Bounds.Left, Y + Frame->GetBoundsRect().Top - Bounds.Top, Clip, 0, 255);''', 1)
    if source.name == "GI_GI.cpp":
        draw_call = "                        Image->Image->DrawToGraphBuf(GR_Main::ScreenRenderBuffer, X, Y, ClipRect, 0, Alpha);"
        if text.count(draw_call) != 1:
            raise RuntimeError("GI draw profiling boundary changed")
        text = text.replace(draw_call, '''                        const auto e2e_gi_draw_begin = std::chrono::steady_clock::now();
                        Image->Image->DrawToGraphBuf(GR_Main::ScreenRenderBuffer, X, Y, ClipRect, 0, Alpha);
                        const auto e2e_gi_draw_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - e2e_gi_draw_begin).count();
#if defined(E2E5_HOST_SDL_WINDOW)
                        constexpr auto e2e_gi_slow_us = 5000;
#else
                        constexpr auto e2e_gi_slow_us = 30000;
#endif
                        static std::int32_t e2e_gi_log_screen = -1;
                        static std::uint32_t e2e_gi_log_count = 0;
                        if (e2e_gi_log_screen != GlobalsV::CurrentScreenId) {
                            e2e_gi_log_screen = GlobalsV::CurrentScreenId;
                            e2e_gi_log_count = 0;
                        }
                        if (e2e_gi_draw_us >= e2e_gi_slow_us && e2e_gi_log_count < 8) {
                            const pas::AnsiString e2e_gi_key = static_cast<pas::AnsiString>(Image->CacheKey);
                            char e2e_gi_detail[220]{};
                            std::snprintf(e2e_gi_detail, sizeof(e2e_gi_detail),
                                "gi draw screen=%d key=%.80s format=%d us=%lld clip=%d,%d,%d,%d",
                                GlobalsV::CurrentScreenId, e2e_gi_key.c_str(), Image->Image->Header->Format,
                                static_cast<long long>(e2e_gi_draw_us), ClipRect.Left, ClipRect.Top,
                                ClipRect.Right, ClipRect.Bottom);
                            srhd_awa::platform::e2e_stage::Log(e2e_gi_detail);
                            ++e2e_gi_log_count;
                        }''', 1)
        text = '#include "e2e_stage.hpp"\n#include <chrono>\n#include <cstdio>\n' + text
    if source.name == "aGalaxy.cpp":
        start = text.index("    void TGalaxy::NextDay() {")
        end = text.index("\n    void TGalaxy::", start + 1)
        next_day = text[start:end]
        begin = "        std::int32_t Stage = 0;"
        star_begin = "                for (auto cpp_range = pas::for_to<std::int32_t>(0, pas::list_count(Stars) - 1); cpp_range.next(I); ) {"
        star_end = "                Stage = 2;"
        finish = "                Stage = 26;\n                SE_Garbage::CheckMemoryUsage();"
        if any(next_day.count(anchor) != 1 for anchor in (begin, star_begin, star_end, finish)):
            raise RuntimeError("galaxy turn profiling boundary changed")
        next_day = next_day.replace(begin, begin + '''
        const auto e2e_galaxy_turn_begin = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point e2e_star_begin;
        std::uint64_t e2e_star_us = 0;''', 1)
        next_day = next_day.replace(star_begin,
            "                e2e_star_begin = std::chrono::steady_clock::now();\n" + star_begin, 1)
        next_day = next_day.replace(star_end, '''                e2e_star_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - e2e_star_begin).count();
''' + star_end, 1)
        next_day = next_day.replace(finish, finish + '''
                static std::uint64_t e2e_star_sum_us = 0;
                static std::uint64_t e2e_turn_sum_us = 0;
                static std::uint32_t e2e_turn_count = 0;
                e2e_star_sum_us += e2e_star_us;
                e2e_turn_sum_us += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - e2e_galaxy_turn_begin).count();
                if (++e2e_turn_count == 25) {
                    char e2e_detail[160]{};
                    std::snprintf(e2e_detail, sizeof(e2e_detail),
                        "galaxy turn segment=25 star_us=%llu total_us=%llu stars=%d",
                        static_cast<unsigned long long>(e2e_star_sum_us),
                        static_cast<unsigned long long>(e2e_turn_sum_us), pas::list_count(Stars));
                    srhd_awa::platform::e2e_stage::Log(e2e_detail);
                    e2e_star_sum_us = e2e_turn_sum_us = 0;
                    e2e_turn_count = 0;
                }''', 1)
        text = text[:start] + next_day + text[end:]
        text = '#include "e2e_stage.hpp"\n#include <chrono>\n#include <cstdio>\n#include <cstdint>\n' + text
    if source.name == "fCfgSettings.cpp":
        marker = "    void TfCfgSettings::OnOpen() {"
        if text.count(marker) != 1:
            raise RuntimeError("Options opening boundary changed")
        missing_option = '''        GR_Main::RaiseWideMessage(pas::concat_wide({u"UnitGet Type=", OptionName}));'''
        if text.count(missing_option) != 1:
            raise RuntimeError("Options value lookup boundary changed")
        text = text.replace(missing_option, '''        // A fixed Switch display mode is shown as a label, so it has no
        // interactive choice or slider for GetOptionValue to find on Save.
        if (OptionName == u"Resolution" && GR_Main::GameDisplayModeCount == 1)
            return GR_Main::SelectedGameDisplayMode;
        if (OptionName == u"RobotResolution" && GR_Main::RobotDisplayModeCount == 1)
            return GR_Main::SelectedRobotDisplayMode;
''' + missing_option, 1)
        language_choice = '''            Language = EC_Str::ExtractDelimitedPartW(pas::view(GR_Main::AvailableLanguageCodes), GetOptionValue(u"Lang"sv), u","sv);'''
        if text.count(language_choice) != 1:
            raise RuntimeError("Options language choice boundary changed")
        text = text.replace(language_choice, language_choice + '''
            // Host filesystems may not match INSTALL_*.txt case-insensitively.
            // Keep the already loaded language if no installed-code list was found.
            if (Language == u"") Language = GR_Main::SelectedLanguage;''', 1)
        resolution_branch = "        if (GR_Main::AltResolutionSwitch) {"
        if text.count(resolution_branch) != 2:
            raise RuntimeError("Options resolution choice boundaries changed")
        text = text.replace(resolution_branch, '''        if (GR_Main::GameDisplayModeCount == 1) {
            ValueLabel = AddOptionLabel(u"Resolution"_w, aConst::LocalizedText(u"FormCfgSettings.Resolution"_wref.get()), false);
            const auto& e2e_mode = GR_Main::GameDisplayModes[0];
            ValueLabel->SetText(pas::concat_wide({ValueLabel->HelpText, aMyFunction::TextHighlightColorTag, u" ",
                pas::wide_int64_to_str(e2e_mode.Width), u"x", pas::wide_int64_to_str(e2e_mode.Height),
                aMyFunction::EndColorTag}));
        } else if (GR_Main::AltResolutionSwitch) {''', 1)
        text = text.replace(resolution_branch, '''        if (GR_Main::RobotDisplayModeCount == 1) {
            ValueLabel = AddOptionLabel(u"RobotResolution"_w, aConst::LocalizedText(u"FormCfgSettings.Resolution"_wref.get()), false);
            const auto& e2e_mode = GR_Main::RobotDisplayModes[0];
            ValueLabel->SetText(pas::concat_wide({ValueLabel->HelpText, aMyFunction::TextHighlightColorTag, u" ",
                pas::wide_int64_to_str(e2e_mode.Width), u"x", pas::wide_int64_to_str(e2e_mode.Height),
                aMyFunction::EndColorTag}));
        } else if (GR_Main::AltResolutionSwitch) {''', 1)
        text = text.replace(marker, marker + '''
        char e2e_modes[256]{};
        std::snprintf(e2e_modes, sizeof(e2e_modes),
            "options modes game=%d selected=%d length=%d robot=%d selected_robot=%d robot_length=%d current=%dx%d",
            GR_Main::GameDisplayModeCount, GR_Main::SelectedGameDisplayMode,
            GR_Main::GameDisplayModes.length(), GR_Main::RobotDisplayModeCount,
            GR_Main::SelectedRobotDisplayMode, GR_Main::RobotDisplayModes.length(),
            GR_Main::GameScreenWidth, GR_Main::GameScreenHeight);
        srhd_awa::platform::e2e_stage::Log(e2e_modes);
        if (GR_Main::GameDisplayModeCount <= 0 ||
            GR_Main::GameDisplayModeCount > GR_Main::GameDisplayModes.length() ||
            GR_Main::SelectedGameDisplayMode < 0 ||
            GR_Main::SelectedGameDisplayMode >= GR_Main::GameDisplayModeCount ||
            GR_Main::GameScreenWidth <= 0 || GR_Main::GameScreenHeight <= 0 ||
            GR_Main::RobotDisplayModeCount <= 0 ||
            GR_Main::RobotDisplayModeCount > GR_Main::RobotDisplayModes.length() ||
            GR_Main::SelectedRobotDisplayMode < 0 ||
            GR_Main::SelectedRobotDisplayMode >= GR_Main::RobotDisplayModeCount) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=options display-modes");
            pas::raise(pas::make_exception<pas::Exception>("Options display modes unavailable"_a));
        }
''', 1)
        group_done = "        RefreshVisibleGroup();\n        RefreshModeUi();\n        if (aGalaxy::Galaxy != nullptr) {"
        if text.count(group_done) != 1:
            raise RuntimeError("Options group construction boundary changed")
        text = text.replace(group_done, '''        RefreshVisibleGroup();
        RefreshModeUi();
        auto e2e_log_group_button = [&](int index, GI_MessageLoop::TObjectGI* button) {
            char detail[160]{};
            std::snprintf(detail, sizeof(detail),
                "options group button=%d bounds=%d,%d,%d,%d",
                index, button->HitTestBounds.Left, button->HitTestBounds.Top,
                button->HitTestBounds.Right, button->HitTestBounds.Bottom);
            srhd_awa::platform::e2e_stage::Log(detail);
        };
        e2e_log_group_button(0, GetByName(u"ButGroup0"sv));
        e2e_log_group_button(1, GetByName(u"ButGroup1"sv));
        e2e_log_group_button(2, GetByName(u"ButGroup2"sv));
        e2e_log_group_button(3, GetByName(u"ButGroup3"sv));
        e2e_log_group_button(4, GetByName(u"ModeLeftButtonN"sv));
        e2e_log_group_button(6, GetByName(u"Ok"sv));
        for (auto* control = GroupPanels[1]->FirstChild; control != nullptr;
             control = control->NextSibling) {
            if (control->ControlName == u"VSync" &&
                control->UserValue != GR_Main::VSyncEnabled &&
                pas::class_cast_if<GI_Image::TImageGI*>(control) != nullptr) {
                char detail[160]{};
                std::snprintf(detail, sizeof(detail),
                    "options choice target=VSync bounds=%d,%d,%d,%d",
                    control->HitTestBounds.Left, control->HitTestBounds.Top,
                    control->HitTestBounds.Right, control->HitTestBounds.Bottom);
                srhd_awa::platform::e2e_stage::Log(detail);
            }
        }
        for (auto* control = GroupPanels[2]->FirstChild; control != nullptr;
             control = control->NextSibling) {
            if (control->ControlName == u"Music" &&
                control->UserValue != GlobalsV::MusicEnabled &&
                pas::class_cast_if<GI_Image::TImageGI*>(control) != nullptr) {
                char detail[160]{};
                std::snprintf(detail, sizeof(detail),
                    "options choice target=Music bounds=%d,%d,%d,%d",
                    control->HitTestBounds.Left, control->HitTestBounds.Top,
                    control->HitTestBounds.Right, control->HitTestBounds.Bottom);
                srhd_awa::platform::e2e_stage::Log(detail);
            }
            if ((control->ControlName == u"SoundVolume" ||
                 control->ControlName == u"MusicVolume") &&
                pas::class_cast_if<GI_CountBar::TCountBarGI*>(control) != nullptr) {
                const pas::AnsiString key = static_cast<pas::AnsiString>(control->ControlName);
                char detail[160]{};
                std::snprintf(detail, sizeof(detail),
                    "options slider target=%.30s bounds=%d,%d,%d,%d",
                    key.c_str(), control->HitTestBounds.Left, control->HitTestBounds.Top,
                    control->HitTestBounds.Right, control->HitTestBounds.Bottom);
                srhd_awa::platform::e2e_stage::Log(detail);
            }
        }
        char e2e_groups[160]{};
        std::snprintf(e2e_groups, sizeof(e2e_groups),
            "options groups constructed heights=%d,%d,%d,%d,%d,%d",
            GroupNextY[0], GroupNextY[1], GroupNextY[2],
            GroupNextY[3], GroupNextY[4], GroupNextY[5]);
        srhd_awa::platform::e2e_stage::Log(e2e_groups);
        if (aGalaxy::Galaxy != nullptr) {''', 1)
        group_clicked = "    void TfCfgSettings::GroupClicked(GI_MessageLoop::TObjectGI* Sender) {"
        if text.count(group_clicked) != 1:
            raise RuntimeError("Options group callback boundary changed")
        group_begin = text.index(group_clicked)
        group_end = text.index("\n    }", group_begin)
        group_body = text[group_begin:group_end]
        group_refresh = "            RefreshVisibleGroup();\n            RefreshModeUi();"
        if group_body.count(group_refresh) != 1:
            raise RuntimeError("Options group activation boundary changed")
        group_body = group_body.replace(group_refresh, group_refresh + '''
            char e2e_group[64]{};
            std::snprintf(e2e_group, sizeof(e2e_group),
                "options active group=%d", ActiveGroupIndex);
            srhd_awa::platform::e2e_stage::Log(e2e_group);''', 1)
        text = text[:group_begin] + group_body + text[group_end:]
        choice_start = "    void TfCfgSettings::OptionChoiceMouseDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {"
        if text.count(choice_start) != 1:
            raise RuntimeError("Options choice callback boundary changed")
        choice_begin = text.index(choice_start)
        choice_end = text.index("\n    }", choice_begin)
        choice_body = text[choice_begin:choice_end]
        choice_normalized = "        GI_MessageLoop::TObjectGI* Control = GroupPanels[ActiveGroupIndex]->FirstChild;"
        if choice_body.count(choice_normalized) != 1:
            raise RuntimeError("Options choice normalization boundary changed")
        choice_body = choice_body.replace(choice_normalized, '''        if (Sender != nullptr && Sender->ControlName == u"VSync") {
            char detail[80]{};
            std::snprintf(detail, sizeof(detail),
                "options choice selected VSync=%lld",
                static_cast<long long>(Sender->UserValue));
            srhd_awa::platform::e2e_stage::Log(detail);
        }
''' + choice_normalized, 1)
        text = text[:choice_begin] + choice_body + text[choice_end:]
        mode_start = "    void TfCfgSettings::ModeMouseUp(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {"
        if text.count(mode_start) != 1:
            raise RuntimeError("Options mode callback boundary changed")
        mode_begin = text.index(mode_start)
        mode_end = text.index("\n    }", mode_begin)
        mode_body = text[mode_begin:mode_end]
        mode_anchor = "        RefreshModeUi();"
        if mode_body.count(mode_anchor) != 1:
            raise RuntimeError("Options mode refresh boundary changed")
        mode_body = mode_body.replace(mode_anchor, '''        char e2e_mode[80]{};
        std::snprintf(e2e_mode, sizeof(e2e_mode),
            "options mode=%d", SettingsMode);
        srhd_awa::platform::e2e_stage::Log(e2e_mode);
''' + mode_anchor, 1)
        text = text[:mode_begin] + mode_body + text[mode_end:]
        slider_pointer = "Slider->UserIndex = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(ValueLabel));"
        choice_pointer = "ValueLabel->UserValue = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Image));"
        slider_label = "ValueLabel = reinterpret_cast<GI_Label::TLabelGI*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Sender->UserIndex)));"
        slider_guard = "if (Sender->UserIndex != 0) {\n            " + slider_label
        choice_lookup = "Sender = reinterpret_cast<GI_MessageLoop::TObjectGI*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Sender->UserValue)));"
        if (text.count(slider_pointer) != 1 or text.count(choice_pointer) != 1 or \
                text.count(slider_guard) != 8 or text.count(choice_lookup) != 3):
            raise RuntimeError("Options native-width callback boundaries changed")
        text = text.replace(slider_pointer, "Slider->UserIndex = 0;", 1)
        text = text.replace(choice_pointer, "ValueLabel->UserValue = 0;", 1)
        text = text.replace(slider_guard,
            "if (Sender != nullptr) {\n            "
            "ValueLabel = pas::class_cast_if<GI_Label::TLabelGI*>(Sender->PrevSibling);\n"
            "            if (ValueLabel == nullptr || ValueLabel->Parent != Sender->Parent) return;", 8)
        text = text.replace(choice_lookup,
            "auto* e2e_label = Sender;\n"
            "            Sender = pas::class_cast_if<GI_Image::TImageGI*>(e2e_label->NextSibling);\n"
            "            if (Sender == nullptr || Sender->Parent != e2e_label->Parent) return;", 3)
        dx_reset = '''        if (ResetNeeded && static_cast<std::uint8_t>(RestartNeeded ^ 1)) {
            GR_Main::GR_DXReset();
        }'''
        if text.count(dx_reset) != 1:
            raise RuntimeError("Options display reset boundary changed")
        text = text.replace(dx_reset, '''        if (ResetNeeded && static_cast<std::uint8_t>(RestartNeeded ^ 1)) {
#if defined(SRHD_PORTABLE_RENDERER)
            if (!srhd_awa::platform::renderer_platform::ApplyVSyncSetting(GR_Main::VSyncEnabled))
                RestartNeeded = true;
#else
            GR_Main::GR_DXReset();
#endif
        }''', 1)
        text = '#include "renderer_platform.hpp"\n#include "e2e_stage.hpp"\n#include <cstdio>\n' + text
    if source.name == "fGameSettings2.cpp":
        text = '#include "e2e5_text_input.hpp"\n' + text
        text = _replace_exact(text, 'namespace fGameSettings2 {', '''namespace fGameSettings2 {
    void E2EArmNameKeyboard(GI_MessageLoop::TObjectGI* Sender,
                            std::uint32_t, WindowsSdk::TPoint Point) {
        Sender->UserState = Sender->ContainsPoint(Point) ? 1 : 0;
    }
''', 1, source.name)
        text = _replace_exact(text, '            PlayerName->MaxLength = 13;',
            '            PlayerName->MaxLength = 13;\n            PlayerName->LeftButtonDownCallback = pas::bind_static_method<&E2EArmNameKeyboard>(this);\n            PlayerName->LeftButtonUpCallback = pas::bind_method<&TfGameSettings2::PlayerNameMouseDown>(this);', 1, source.name)
        callback_start = text.index('    void TfGameSettings2::PlayerNameMouseDown(')
        callback_end = text.index('    void TfGameSettings2::ToggleLevelPanel(', callback_start)
        callback = text[callback_start:callback_end]
        callback = _replace_exact(callback, '        SetFocusedControl(GetByName(u"PlayerName"sv));', '''        auto* field = GetByName(u"PlayerName"sv);
        SetFocusedControl(field);
        const bool activate = Sender == field && field->UserState == 1 && field->ContainsPoint(Point);
        field->UserState = 0;
        if (activate) {
            auto* edit = pas::checked_cast<GI_Edit::TEditGI*>(Sender);
            auto name = edit->Text;
            if (srhd_awa::platform::e2e5_text_input::EditPlayerName(name)) {
                edit->SetText(name);
                PlayerNameChanged(edit);
            }
        }''', 1, source.name)
        text = text[:callback_start] + callback + text[callback_end:]
        # The translated 32-bit UI stores object pointers in UserIndex/UserValue.
        # These controls are created as adjacent siblings, so resolve the live
        # object from the UI tree instead of narrowing its address on Switch.
        old_slider = "Slider->UserIndex = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(ValueLabel));"
        if text.count(old_slider) != 1:
            raise RuntimeError("extended slider label boundary changed")
        text = text.replace(old_slider, "Slider->UserIndex = 0;", 1)
        for field in ("cpp_with_2->UserValue = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(IronWillImage));",
                      "CaptionLabel->UserValue = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(ChoiceImage));"):
            if text.count(field) != 1:
                raise RuntimeError("extended choice pointer storage changed")
            text = text.replace(field, field.split(" = ")[0] + " = 0;", 1)
        old_label = "GI_Label::TLabelGI* cpp_with = reinterpret_cast<GI_Label::TLabelGI*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Sender->UserIndex)));"
        if text.count(old_label) != 4:
            raise RuntimeError("extended slider callbacks changed")
        text = text.replace("if (Sender->UserIndex != 0) {\n            " + old_label,
                            "if (Sender != nullptr) {\n            GI_Label::TLabelGI* cpp_with = pas::class_cast_if<GI_Label::TLabelGI*>(Sender->PrevSibling);\n            if (cpp_with == nullptr || cpp_with->Parent != Sender->Parent) return;", 4)
        old_choice = "Sender = reinterpret_cast<GI_MessageLoop::TObjectGI*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Sender->UserValue)));"
        choice_start = "    void TfGameSettings2::ExtendedChoiceMouseDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {"
        choice_boundary = "        if (!(pas::class_cast_if<GI_Image::TImageGI*>(Sender) != nullptr)) {\n            " + old_choice
        if text.count(choice_start) != 1:
            raise RuntimeError("extended choice callback changed")
        choice_begin = text.index(choice_start)
        choice_end = text.index("\n    }", choice_begin)
        choice_body = text[choice_begin:choice_end]
        if choice_body.count(choice_boundary) != 1:
            raise RuntimeError("extended choice pointer boundary changed")
        choice_body = choice_body.replace(choice_boundary,
            "        if (!(pas::class_cast_if<GI_Image::TImageGI*>(Sender) != nullptr)) {\n            Sender = pas::class_cast_if<GI_Image::TImageGI*>(Sender->NextSibling);\n            if (Sender == nullptr) return;", 1)
        text = text[:choice_begin] + choice_body + text[choice_end:]
        for method in ("IronWillMouseDown", "IronWillMouseEnter", "IronWillMouseLeave"):
            marker = "    void TfGameSettings2::" + method + "("
            if text.count(marker) != 1:
                raise RuntimeError("IronWill callback changed: " + method)
            begin = text.index(marker)
            end = text.index("\n    }", begin)
            body = text[begin:end]
            if body.count(old_choice) != 1:
                raise RuntimeError("IronWill pointer boundary changed: " + method)
            body = body.replace(old_choice,
                "Sender = pas::class_cast_if<GI_Image::TImageGI*>(Sender->PrevSibling);\n            if (Sender == nullptr) return;", 1)
            text = text[:begin] + body + text[end:]
        start = "        Globals::NewGameGenerationThread->Start();"
        if text.count(start) != 1:
            raise RuntimeError("new game worker start boundary changed")
        text = text.replace(start,
            "        srhd_awa::platform::e2e3_generation_state::Reset();\n" + start +
            '\n        srhd_awa::platform::e2e_stage::LogE2E3("generation worker=start-requested");', 1)
        open_method = "    void TfGameSettings2::OnOpen() {"
        saved = "                GR_Main::NewGameSettingsConfig->SaveTextFile(FileName.pchar(), true, false);"
        if text.count(open_method) != 1 or text.count(saved) != 1:
            raise RuntimeError("GameSettings open/save boundary changed")
        text = text.replace(open_method, open_method +
            '\n        srhd_awa::platform::e2e_stage::LogE2E3("game_settings status=open");', 1)
        text = text.replace(saved, saved +
            '\n                srhd_awa::platform::e2e_stage::LogE2E3("game_settings action=apply settings=saved");', 1)
        text = '#include "e2e3_generation_state.hpp"\n' + text
    if source.name == "fGameSettings.cpp":
        stage_pattern = r"fIntroduction::NewGameGenerationStage = ([0-8]);"
        stages = re.findall(stage_pattern, text)
        if stages != ["0", "1", "2", "3", "4", "5", "6", "8"]:
            raise RuntimeError(f"new game generation stages changed: {stages}")
        text = re.sub(stage_pattern,
            lambda match: "srhd_awa::platform::e2e3_generation_state::SetStage(" +
                          match.group(1) + ");", text)
        old_failure = '''                GR_Main::AppendLogLineThreadSafe(pas::concat_ansi({"Galaxy create exception, label = ", SysUtils::IntToStr(Stage), " seed = ", SysUtils::IntToStr(aGalaxy::Galaxy->GenerationSeed)}));
            } else {
                throw;
            }
        }'''
        new_failure = '''                GR_Main::AppendLogLineThreadSafe(pas::concat_ansi({"Galaxy create exception, label = ", SysUtils::IntToStr(Stage), " seed = ", aGalaxy::Galaxy != nullptr ? SysUtils::IntToStr(aGalaxy::Galaxy->GenerationSeed) : pas::AnsiString("unavailable")}));
            } else {
                GR_Main::AppendLogLineThreadSafe("Galaxy create exception, non-game exception"_a);
            }
            srhd_awa::platform::e2e3_generation_state::Fail(Stage);
        }'''
        if text.count(old_failure) != 1:
            raise RuntimeError("new game generation exception boundary changed")
        text = text.replace(old_failure, new_failure, 1)
        warmup_begin = '''            srhd_awa::platform::e2e3_generation_state::SetStage(6);
            Stage = 17;
            aCalc::CalculateGalaxyTurnAndWait();'''
        warmup_loop = '''            for (I = 1; I <= aGalaxyStruct::GalaxyWarmupTurns; ++I) {
                if (I % 20 == 0) {
                    SysUtilsImports::Sleep(1u);
                }
                aCalc::CalculatePlayerStarTurnAndWait();'''
        warmup_end = '''                aCalc::CalculateGalaxyTurnAndWait();
                if (GR_Main::ExitScreenLoop) {
                    return;
                }
            }
            Stage = 18;'''
        # This section runs hundreds of turns on a worker. Record sparse
        # checkpoints so a console log identifies the exact stalled turn.
        if text.count(warmup_begin) != 1 or text.count(warmup_loop) != 1 or text.count(warmup_end) != 1:
            raise RuntimeError("new game warmup boundaries changed")
        text = text.replace(warmup_begin, '''            srhd_awa::platform::e2e3_generation_state::SetStage(6);
            Stage = 17;
            srhd_awa::platform::e2e_stage::Log("generation warmup initial-galaxy-turn begin");
            aCalc::CalculateGalaxyTurnAndWait();
            srhd_awa::platform::e2e_stage::Log("generation warmup initial-galaxy-turn done");''', 1)
        text = text.replace(warmup_loop, '''            std::uint64_t e2e_player_turn_us = 0;
            std::uint64_t e2e_galaxy_turn_us = 0;
            for (I = 1; I <= aGalaxyStruct::GalaxyWarmupTurns; ++I) {
                if (I == 1 || I % 25 == 0) {
                    char detail[96]{};
                    std::snprintf(detail, sizeof(detail), "generation warmup turn=%d/%d begin",
                        I, aGalaxyStruct::GalaxyWarmupTurns);
                    srhd_awa::platform::e2e_stage::Log(detail);
                }
                if (I % 20 == 0) {
                    SysUtilsImports::Sleep(1u);
                }
                const auto e2e_player_turn_begin = std::chrono::steady_clock::now();
                aCalc::CalculatePlayerStarTurnAndWait();
                e2e_player_turn_us += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - e2e_player_turn_begin).count();''', 1)
        text = text.replace(warmup_end, '''                const auto e2e_galaxy_turn_begin = std::chrono::steady_clock::now();
                aCalc::CalculateGalaxyTurnAndWait();
                e2e_galaxy_turn_us += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - e2e_galaxy_turn_begin).count();
                if (GR_Main::ExitScreenLoop) {
                    return;
                }
                if (I == 1 || I % 25 == 0) {
                    char detail[96]{};
                    std::snprintf(detail, sizeof(detail), "generation warmup turn=%d/%d done",
                        I, aGalaxyStruct::GalaxyWarmupTurns);
                    srhd_awa::platform::e2e_stage::Log(detail);
                }
                if (I % 25 == 0) {
                    char detail[128]{};
                    std::snprintf(detail, sizeof(detail),
                        "generation warmup segment=%d player_us=%llu galaxy_us=%llu",
                        I / 25, static_cast<unsigned long long>(e2e_player_turn_us),
                        static_cast<unsigned long long>(e2e_galaxy_turn_us));
                    srhd_awa::platform::e2e_stage::Log(detail);
                    e2e_player_turn_us = e2e_galaxy_turn_us = 0;
                }
            }
            Stage = 18;''', 1)
        text = '#include "e2e_stage.hpp"\n#include <chrono>\n#include <cstdio>\n#include <cstdint>\n#include "e2e3_generation_state.hpp"\n' + text
    if source.name == "fIntroduction.cpp":
        text = '#include "units/MMSystem.hpp"\n' + text
        text = _replace_exact(text,
            'ScheduleCallbackTimer(40, 40, pas::bind_method<&TfIntroduction::ScrollBackground>(this), 0)',
            'ScheduleCallbackTimer(16, 16, pas::bind_method<&TfIntroduction::ScrollBackground>(this), static_cast<std::int32_t>(MMSystem::timeGetTime()))',
            1, source.name)
        text = _replace_exact(text, '        ++BackgroundScrollOffset;', '''        // Preserve 25 pixels/second while catching up after a delayed callback.
        // Store the remainder in the timer epoch, including uint32 tick wrap.
        const auto now = MMSystem::timeGetTime();
        const auto previous = Timer ? static_cast<std::uint32_t>(Timer->UserData) : now;
        const auto steps = static_cast<std::uint32_t>(now - previous) / 40u;
        if (Timer) Timer->UserData = static_cast<std::int32_t>(previous + steps * 40u);
        const auto width = GetByName(u"ImageFon1"sv)->ClientSize.X;
        if (width <= 0) return;
        BackgroundScrollOffset = static_cast<std::int32_t>(
            (static_cast<std::uint64_t>(BackgroundScrollOffset) + steps) %
            static_cast<std::uint32_t>(width));''', 1, source.name)
        stage_read = "DisplayedGenerationStage < NewGameGenerationStage"
        if text.count(stage_read) != 1:
            raise RuntimeError("Introduction generation stage read boundary changed")
        text = text.replace(stage_read,
            "DisplayedGenerationStage < srhd_awa::platform::e2e3_generation_state::Stage()", 1)
        timer = "    void TfIntroduction::UpdateGenerationProgress(GI_MessageLoop::PCallbackTimerGI Timer, std::int32_t UserData) {"
        if text.count(timer) != 1:
            raise RuntimeError("Introduction progress timer boundary changed")
        text = text.replace(timer, timer + '''
        if (Globals::NewGameGenerationThread != nullptr &&
            srhd_awa::platform::e2e3_generation_state::Failed()) {
            srhd_awa::platform::e2e_stage::LogE2E3("generation status=failed return=new_game_settings");
            GlobalsV::RequestedScreenId = GlobalsV::screenNewGame;
            RequestClose(1);
            return;
        }''', 1)
        ready = "                Ok->SetDisabled(false);"
        if text.count(ready) != 1:
            raise RuntimeError("Introduction continue-ready boundary changed")
        text = text.replace(ready, ready +
            '\n                srhd_awa::platform::e2e_stage::LogE2E3("introduction status=continue-enabled");', 1)
        open_method = "    void TfIntroduction::OnOpen() {"
        continue_request = "        RequestClose(1);"
        if text.count(open_method) != 1:
            raise RuntimeError("Introduction open boundary changed")
        text = text.replace(open_method, open_method +
            '\n        srhd_awa::platform::e2e_stage::LogE2E3("introduction status=open");', 1)
        continue_start = text.index("    void TfIntroduction::ContinueClicked(")
        continue_end = text.index("    void TfIntroduction::MainPanelKeyDown(", continue_start)
        continuation = text[continue_start:continue_end]
        if continuation.count(continue_request) != 1:
            raise RuntimeError("Introduction continue boundary changed")
        continuation = continuation.replace(continue_request,
            '''        {
            char detail[80]{};
            std::snprintf(detail, sizeof(detail), "introduction action=continue requested=%u",
                          static_cast<unsigned>(GlobalsV::RequestedScreenId));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }
''' + continue_request, 1)
        text = text[:continue_start] + continuation + text[continue_end:]
        text = '#include "e2e3_generation_state.hpp"\n' + text
    if e2e5_profile and source.name in ("fRuinsTalk.cpp", "fTalk.cpp"):
        text = replace_dialog_pointer_payloads(text, source.name)
    if audio_on and source.name == "fLoad.cpp":
        load_begin = text.index("    void TfLoad::OnOpen() {")
        load_end = text.index("    void TfLoad::OnClose() {", load_begin)
        load = text[load_begin:load_end]
        fade = '''        if (IntroFinished && GR_Main::MusicManager->CategoryOverride == u"") {'''
        if load.count(fade) != 1:
            raise RuntimeError("loading-screen music fade boundary changed")
        load = load.replace(fade, '''        // Keep the hangar track during the potentially long StarMap load.
        // StarMap selects its new category after loading; the music controller
        // fades this track once the next PCM ring is ready.
        if (IntroFinished && GR_Main::MusicManager->CategoryOverride == u"" &&
            GlobalsV::PostLoadScreenId != GlobalsV::screenStarMap) {''', 1)
        text = text[:load_begin] + load + text[load_end:]
    if audio_on and source.name == "fHangar.cpp":
        takeoff_begin = text.index("    void TfHangar::TakeOffClicked(")
        takeoff_end = text.index("    void TfHangar::StopAnimation(", takeoff_begin)
        takeoff = text[takeoff_begin:takeoff_end]
        fade = '''        if (GR_Main::MusicManager->CategoryOverride == u"") {
            GR_Main::MusicManager->RequestFadeOut();
        }'''
        if takeoff.count(fade) != 1:
            raise RuntimeError("hangar takeoff music fade boundary changed")
        takeoff = takeoff.replace(fade,
            "        // Retain the current track through the StarMap loading screen.\n", 1)
        text = text[:takeoff_begin] + takeoff + text[takeoff_end:]
    if e2e5_profile and source.name == "fHangar.cpp":
        truncated = ("std::uint32_t cpp_left = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(GR_Main::ScreenRenderBuffer->GetPixels())) + static_cast<std::uint32_t>(Position.X * 2);\n"
                     "                void* cpp_arg = reinterpret_cast<void*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(cpp_left + static_cast<std::uint32_t>(Position.Y * GR_Main::ScreenRenderBuffer->PitchBytes))));")
        if text.count(truncated) != 1:
            raise RuntimeError("Hangar screen-buffer pointer boundary changed")
        text = text.replace(truncated,
            "std::uint8_t* cpp_left = static_cast<std::uint8_t*>(GR_Main::ScreenRenderBuffer->GetPixels()) + Position.X * 2;\n"
            "                void* cpp_arg = cpp_left + Position.Y * GR_Main::ScreenRenderBuffer->PitchBytes;", 1)
        turn_call = '        MainPanel->EndTurnClicked(Sender);'
        if text.count(turn_call) != 1:
            raise RuntimeError("original Hangar end-turn boundary changed")
        text = text.replace(turn_call, turn_call + '''
        {
            char detail[96]{};
            std::snprintf(detail, sizeof(detail), "hangar_end_turn_done turn=%u",
                          static_cast<unsigned>(aGalaxy::Galaxy->CurrentTurn));
            srhd_awa::platform::e2e_stage::LogE2E3(detail);
        }''', 1)
        text = '#include "e2e_stage.hpp"\n#include <cstdio>\n' + text
    if source.name == "GR_GraphBuf.cpp":
        rgba_start = "    void TGraphBufGR::AllocateRgbaTight(std::int32_t Width, std::int32_t Height) {"
        if text.count(rgba_start) != 1:
            raise RuntimeError("RGBA allocation boundary changed")
        text = text.replace(rgba_start, rgba_start + '''
        const std::int64_t e2e_bytes = static_cast<std::int64_t>(Width) * Height * 4;
        if (Width <= 0 || Height <= 0 || Width > 16384 || Height > 16384 ||
            e2e_bytes <= 0 || e2e_bytes > 256LL * 1024 * 1024) {
            char e2e_detail[160]{};
            std::snprintf(e2e_detail, sizeof(e2e_detail),
                "FAIL stage=rgba-allocate width=%d height=%d pitch64=%lld bytes64=%lld",
                Width, Height, static_cast<long long>(Width) * 4,
                static_cast<long long>(e2e_bytes));
            srhd_awa::platform::e2e_stage::Log(e2e_detail);
            pas::raise(pas::make_exception<pas::Exception>("invalid RGBA dimensions"_a));
        }
''', 1)
        aspect_start = "    void TGraphBufGR::RescaleWithAspect(std::uint32_t Width, std::uint32_t Height, std::uint8_t CropToAspect, std::int32_t HorizontalAlign, std::int32_t VerticalAlign, std::int32_t Filter) {"
        if text.count(aspect_start) != 1:
            raise RuntimeError("aspect rescale boundary changed")
        text = text.replace(aspect_start, aspect_start + '''
        const std::uint64_t e2e_bytes = static_cast<std::uint64_t>(Width) * Height *
            static_cast<std::uint64_t>(BytesPerPixel > 0 ? BytesPerPixel : 0);
        if (Width == 0 || Height == 0 || Width > 16384 || Height > 16384 ||
            this->Width <= 0 || this->Height <= 0 || PitchBytes <= 0 ||
            BytesPerPixel <= 0 || BytesPerPixel > 4 ||
            e2e_bytes == 0 || e2e_bytes > 256ULL * 1024 * 1024) {
            char e2e_detail[192]{};
            std::snprintf(e2e_detail, sizeof(e2e_detail),
                "FAIL stage=aspect-rescale source=%dx%d pitch=%d bpp=%d target=%ux%u bytes64=%llu",
                this->Width, this->Height, PitchBytes, BytesPerPixel,
                Width, Height, static_cast<unsigned long long>(e2e_bytes));
            srhd_awa::platform::e2e_stage::Log(e2e_detail);
            pas::raise(pas::make_exception<pas::Exception>("invalid rescale dimensions"_a));
        }
''', 1)
        text = '#include "e2e_stage.hpp"\n#include <cstdio>\n#include <cstdint>\n' + text
        use_texture = "        Self->UseTexture = AUseTexture;"
        if text.count(use_texture) != 1:
            raise RuntimeError("graph buffer texture preference boundary changed")
        text = text.replace(use_texture, '''#if defined(__SWITCH__) || defined(E2E5_HOST_SDL_WINDOW)
        // The portable software renderer has no Direct3D texture device.
        Self->UseTexture = false;
#else
        Self->UseTexture = AUseTexture;
#endif''', 1)
    if source.name == "aSaveLoad.cpp":
        save_entry = "    std::uint8_t SaveGameToFile(pas::WideString FileName, pas::WideString Description) {"
        if text.count(save_entry) != 1:
            raise RuntimeError("disk save entry boundary changed")
        text = text.replace(save_entry, save_entry + '''
#if defined(E2E5_DISABLE_DISK_SAVE)
        // No disk save mode has completed a separate-process round trip yet.
        // Refuse before serialization, queueing, or touching an existing file.
        GR_Main::AppendLogLineThreadSafe("E2E5 disk save refused pending save/load validation"_a);
        return false;
#endif''', 1)
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
        writer_init = '''    void InitializeSaveWriter() {
        SaveWriter = pas::construct_call<TSaver>(EC_Thread::TThreadEC_Create);
    }'''
        if text.count(writer_init) != 1:
            raise RuntimeError("quicksave startup recovery boundary changed")
        text = text.replace(writer_init, '''    void InitializeSaveWriter() {
#if defined(E2E4_QUICKSAVE_EXPERIMENT) && !defined(E2E5_DISABLE_DISK_SAVE)
        const auto quick = fSaveManager::TfSaveManager::GetQuickSavePath(1);
        const auto recovery = srhd_awa::platform::e2e4_quicksave::RecoverGamePaths(quick.pchar());
        const auto recovery_log = std::string("E2E4 quicksave startup recovery: ") +
            recovery.detail + " backup=" + recovery.backup_directory.string();
        GR_Main::AppendLogLineThreadSafe(pas::AnsiString(recovery_log.c_str()));
        if (!recovery.ok) {
            pas::raise(pas::make_exception<pas::Abort>("E2E4 quicksave recovery failed; inspect retained snapshots"_a));
        }
#endif
        SaveWriter = pas::construct_call<TSaver>(EC_Thread::TThreadEC_Create);
    }''', 1)
        saver_start = text.index("    void TSaver_Execute(TSaver* Self) {")
        saver_end = text.index("    void TSaver::QueueSave(", saver_start)
        saver = text[saver_start:saver_end]
        temp_delete = '''            if (SysUtilsImports::FileExists(static_cast<pas::AnsiString>(TempName))) {
                WindowsSdk::DeleteFile((cpp_text = static_cast<pas::AnsiString>(TempName), cpp_text.pchar()));
            }'''
        if saver.count(temp_delete) != 1:
            raise RuntimeError("quicksave temporary-file boundary changed")
        saver = saver.replace(temp_delete, '''#if defined(E2E4_QUICKSAVE_EXPERIMENT)
            if (Self->FileName == QuickName) {
                if (!srhd_awa::platform::e2e4_quicksave::CanCreateGameTemp(TempName.pchar())) {
                    pas::raise(pas::make_exception<pas::Abort>("E2E4 quicksave: save.tmp exists or path is unsafe"_a));
                }
            } else {
#else
            if (Self->FileName == QuickName) {
                pas::raise(pas::make_exception<pas::Abort>("E2E4 quicksave disabled in standard Switch build"_a));
            }
#endif
''' + temp_delete + '''
#if defined(E2E4_QUICKSAVE_EXPERIMENT)
            }
#endif''', 1)
        rotate_begin = saver.index("            SourceName = TempName;")
        rotate_end = saver.index("        } catch (...) {", rotate_begin)
        original_rotation = saver[rotate_begin:rotate_end]
        saver = saver[:rotate_begin] + '''#if defined(E2E4_QUICKSAVE_EXPERIMENT)
            if (Self->FileName == QuickName) {
                const auto transaction = srhd_awa::platform::e2e4_quicksave::RotateGamePaths(
                    TempName.pchar(), QuickName.pchar(), GlobalsV::QuickSaveExtraSlots);
                const auto transaction_log = std::string("E2E4 quicksave: ") +
                    transaction.detail + " backup=" + transaction.backup_directory.string();
                GR_Main::AppendLogLineThreadSafe(pas::AnsiString(transaction_log.c_str()));
                if (!transaction.ok) {
                    pas::raise(pas::make_exception<pas::Abort>("E2E4 quicksave rotation failed; inspect retained backup directory"_a));
                }
                e2e_quicksave_committed = true;
            } else {
#endif
''' + original_rotation + '''#if defined(E2E4_QUICKSAVE_EXPERIMENT)
            }
#endif
''' + saver[rotate_end:]
        marker = "        EC_File::TFileEC* F = nullptr;"
        if saver.count(marker) != 1:
            raise RuntimeError("quicksave worker state boundary changed")
        saver = saver.replace(marker, '''#if defined(E2E4_QUICKSAVE_EXPERIMENT)
        bool e2e_quicksave_committed = false;
#endif
''' + marker, 1)
        cleanup = '''        if (SysUtilsImports::FileExists(static_cast<pas::AnsiString>(TempName))) {
            WindowsSdk::DeleteFile((cpp_text_4 = static_cast<pas::AnsiString>(TempName), cpp_text_4.pchar()));
        }'''
        if saver.count(cleanup) != 1:
            raise RuntimeError("quicksave post-failure temp cleanup boundary changed")
        saver = saver.replace(cleanup, '''#if defined(E2E4_QUICKSAVE_EXPERIMENT)
        if (Self->FileName != QuickName || e2e_quicksave_committed) {
#else
        if (Self->FileName != QuickName) {
#endif
''' + cleanup + '''
        }''', 1)
        text = text[:saver_start] + saver + text[saver_end:]
        text = '#include "e2e4_quicksave_transaction.hpp"\n' + text
    if source.name == "GR_Main.cpp":
        text = '#include "e2e5_cfg_bootstrap.hpp"\n' + text
        finalize_buffers = "        GR_Main::FreeScreenRenderBuffers();\n        WideCaseTable = nullptr;"
        if text.count(finalize_buffers) != 1:
            raise RuntimeError("portable renderer restart cleanup boundary changed")
        text = text.replace(finalize_buffers, '''#if defined(__SWITCH__) || defined(SRHD_PORTABLE_RENDERER)
        srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer();
#else
        GR_Main::FreeScreenRenderBuffers();
#endif
        WideCaseTable = nullptr;''', 1)
        callback_gate = "                    if (Msg.hwnd == MainWindowHandle && pas::assigned(Callback) && Forms::Application->GetActive()) {"
        if text.count(callback_gate) != 1:
            raise RuntimeError("original input callback gate changed")
        text = text.replace(callback_gate, '''                    const bool e2e_callback_active = Forms::Application->GetActive();
                    static std::uint32_t e2e_move_dispatch_logs = 0;
                    static std::uint32_t e2e_button_dispatch_logs = 0;
                    const bool e2e_is_move = Msg.message == MessagesSdk::WM_MOUSEMOVE;
                    const bool e2e_is_button =
                        Msg.message == MessagesSdk::WM_LBUTTONDOWN ||
                        Msg.message == MessagesSdk::WM_LBUTTONUP ||
                        Msg.message == MessagesSdk::WM_KEYDOWN ||
                        Msg.message == MessagesSdk::WM_QUIT;
                    if ((e2e_is_move && e2e_move_dispatch_logs < 4) ||
                        (e2e_is_button && e2e_button_dispatch_logs < 32)) {
                        char detail[160]{};
                        std::snprintf(detail, sizeof(detail),
                            "input dispatch press_id=%llu message=%u hwnd=%u active=%u callback=%u",
                            static_cast<unsigned long long>(srhd_awa::platform::win32_compat::CurrentPressId()),
                            static_cast<unsigned>(Msg.message), static_cast<unsigned>(Msg.hwnd),
                            static_cast<unsigned>(e2e_callback_active),
                            static_cast<unsigned>(pas::assigned(Callback)));
                        srhd_awa::platform::e2e_stage::Log(detail);
                        if (e2e_is_move) ++e2e_move_dispatch_logs;
                        if (e2e_is_button) ++e2e_button_dispatch_logs;
                    }
                    if (Msg.hwnd == MainWindowHandle && pas::assigned(Callback) && e2e_callback_active) {''', 1)
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
        config_path = '        Text = pas::concat_wide({GR_Main::GetGameUserDirectory(), u"CFG.TXT"});\n        if (!SysUtilsImports::FileExists(static_cast<pas::AnsiString>(Text))) {'
        if text.count(config_copy) != 1 or text.count(config_path) != 1:
            raise RuntimeError("original CFG.TXT creation boundary changed")
        text = text.replace(config_path, '''        auto cfg_result = srhd_awa::platform::e2e5_cfg_bootstrap::Inspect(
            srhd_awa::platform::game_path::Resolve("cfg.txt"),
            srhd_awa::platform::user_root::ResolveConfigPath("CFG.TXT"));
        {
            const std::string detail = "cfg source=" + cfg_result.source +
                " source_type=" + cfg_result.source_type +
                " source_readable=" + std::to_string(cfg_result.source_readable) +
                " target=" + cfg_result.target +
                " target_type=" + cfg_result.target_type +
                " target_readable=" + std::to_string(cfg_result.target_readable) +
                " target_dir=" + std::to_string(cfg_result.target_directory_exists) +
                " space_available=" + std::to_string(cfg_result.space_available) +
                " ec=" + std::to_string(cfg_result.error_value) +
                " ec_message=" + cfg_result.error_message;
            srhd_awa::platform::e2e_stage::Log(detail.c_str());
        }
        if (!cfg_result.used_existing && cfg_result.problem.empty())
            srhd_awa::platform::e2e5_cfg_bootstrap::Create(cfg_result);
        if (!cfg_result.ok()) {
            const std::string failure = "FAIL stage=runtime/settings cfg=" + cfg_result.problem +
                " ec=" + std::to_string(cfg_result.error_value) +
                " ec_message=" + cfg_result.error_message;
            srhd_awa::platform::e2e_stage::Log(failure.c_str());
            pas::raise(pas::make_exception<pas::Exception>(pas::AnsiString(failure.c_str())));
        }
        Text = static_cast<pas::WideString>(pas::AnsiString(cfg_result.target.c_str()));
        if (cfg_result.created) {''', 1)
        text = text.replace(config_copy, '            // The verified template was already copied without overwriting a user file.', 1)
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
            "#if defined(_WIN32) && !defined(__SWITCH__)\n" + dll_checksum_begin, 1)
        text = text.replace(dll_checksum_end,
            "#endif // Windows DLL integrity paths are absent on portable targets.\n" + dll_checksum_end, 1)
        audio_anchor = '        EC_BlockPar::TBlockParEC* Block = LanguageDataConfig->GetBlock(u"CaseConv"sv);'
        if text.count(audio_anchor) != 1:
            raise RuntimeError("original sound initialization boundary changed")
        if not audio_on:
            text = text.replace(audio_anchor,
                "        // OPTIONAL: the first Switch menu run has no audio backend.\n"
                "        GlobalsV::SoundEnabled = false;\n"
                "        GlobalsV::MusicEnabled = false;\n" + audio_anchor, 1)
        else:
            sound_gate = '        if (!GR_Main::IsInstallFeatureEnabled(u"Sound"_wref.get())) {'
            music_gate = '        if (!GR_Main::IsInstallFeatureEnabled(u"Music"_wref.get())) {'
            if text.count(sound_gate) != 1 or text.count(music_gate) != 1:
                raise RuntimeError("audio feature gates changed")
            text = text.replace(sound_gate, '''        // A minimal user CFG omits audio flags. Use the installed feature
        // defaults only for missing values; explicit user choices win.
        if (UserSettingsConfig->CountParamsByPath(u"Sound"_wref.get()) == 0)
            GlobalsV::SoundEnabled = GR_Main::IsInstallFeatureEnabled(u"Sound"_wref.get());
        if (UserSettingsConfig->CountParamsByPath(u"SoundInSpace"_wref.get()) == 0)
            GlobalsV::SoundInSpaceEnabled = GR_Main::IsInstallFeatureEnabled(u"SoundInSpace"_wref.get());
        if (UserSettingsConfig->CountParamsByPath(u"Music"_wref.get()) == 0)
            GlobalsV::MusicEnabled = GR_Main::IsInstallFeatureEnabled(u"Music"_wref.get());
        if (UserSettingsConfig->CountParamsByPath(u"MusicInSpace"_wref.get()) == 0)
            GlobalsV::MusicInSpaceEnabled = GR_Main::IsInstallFeatureEnabled(u"MusicInSpace"_wref.get());
''' + sound_gate, 1)
            text = text.replace(sound_gate, '''        {
            char e2e_audio_detail[192]{};
            std::snprintf(e2e_audio_detail, sizeof(e2e_audio_detail),
                "audio flags before-install sound_cfg_present=%d sound=%d sound_install=%d music_cfg_present=%d music=%d music_install=%d",
                UserSettingsConfig->CountParamsByPath(u"Sound"_wref.get()),
                static_cast<int>(GlobalsV::SoundEnabled),
                static_cast<int>(GR_Main::IsInstallFeatureEnabled(u"Sound"_wref.get())),
                UserSettingsConfig->CountParamsByPath(u"Music"_wref.get()),
                static_cast<int>(GlobalsV::MusicEnabled),
                static_cast<int>(GR_Main::IsInstallFeatureEnabled(u"Music"_wref.get())));
            srhd_awa::platform::e2e_stage::Log(e2e_audio_detail);
        }
''' + sound_gate, 1)
            text = text.replace(music_gate, '''        {
            char e2e_audio_detail[96]{};
            std::snprintf(e2e_audio_detail, sizeof(e2e_audio_detail),
                "audio flags final sound=%d music=%d",
                static_cast<int>(GlobalsV::SoundEnabled),
                static_cast<int>(GlobalsV::MusicEnabled && GR_Main::IsInstallFeatureEnabled(u"Music"_wref.get())));
            srhd_awa::platform::e2e_stage::Log(e2e_audio_detail);
        }
''' + music_gate, 1)
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
        if (!srhd_awa::platform::win32_compat::BindMainWindowProc(MainWindowHandle, &MainWindowProc) ||
            !WindowsSdk::PostMessage(MainWindowHandle, MessagesSdk::WM_ACTIVATEAPP, 1, 0)) {
            pas::raise(pas::make_exception<pas::Exception>("Switch main window activation failed"_a));
        }
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
                 '#include "win32_compat_window.hpp"\n'
                 '#include "win32_compat_messages.hpp"\n'
                '#include "e2e_clock.hpp"\n#include "e2e_stage.hpp"\n'
                '#include "game_path.hpp"\n#include "user_root.hpp"\n'
                '#include <SDL2/SDL.h>\n#if defined(__SWITCH__)\n'
                '#include <switch.h>\n#endif\n#include <thread>\n'
                'namespace { srhd_awa::platform::runtime_platform::State g_e2e_platform; }\n' + text)
    if audio_on and source.name == "GR_Music.cpp":
        text = '#include "e2e5_failure_diagnostics.hpp"\n' + text
        text = _replace_exact(text,
            '            Stream = pas::construct_call<EC_FileStream::TFileStreamEC>(EC_FileStream::TFileStreamEC_Create, 0x000400ff, FileName);',
            '''            srhd_awa::platform::e2e5_failure_diagnostics::Memory("music-load");
            Stream = pas::construct_call<EC_FileStream::TFileStreamEC>(EC_FileStream::TFileStreamEC_Create, 0x000400ff, FileName);''', 1, source.name)
        # A failed filename copy or decoder cleanup must not abandon a mutex
        # owned by a worker: the UI also takes these locks on track changes.
        for signature in ['    void TMusicUnit::Clear() {',
                          '    pas::WideString TMusicUnit::GetFileName() {']:
            begin = text.index(signature)
            end = text.index('\n    }', begin) + len('\n    }')
            body = text[begin:end]
            body = _replace_exact(body, '        pas::critical_enter(DecodeLock);',
                '''        pas::critical_enter(DecodeLock);
        pas::ScopeExit e2e_decode_unlock = [&]() noexcept { pas::critical_leave(DecodeLock); };''', 1, source.name)
            body = _replace_exact(body, '\n        pas::critical_leave(DecodeLock);', '', 1, source.name)
            text = text[:begin] + body + text[end:]
        begin = text.index('    void TMusicControl_Execute(TMusicControl* Self) {')
        end = text.index('    void TMusicControl::PlayFile(', begin)
        body = text[begin:end]
        body = _replace_exact(body, '            pas::critical_enter(Self->ControlLock);',
            '''            try {
            pas::critical_enter(Self->ControlLock);
            pas::ScopeExit e2e_control_unlock = [&]() noexcept { pas::critical_leave(Self->ControlLock); };''', 1, source.name)
        body = _replace_exact(body, '                if (Self->Current->IsRunning()) Self->Current->RequestStop();',
            '''#if defined(E2E5_HOST_SDL_WINDOW)
                static bool e2e_name_failure_injected = false;
                if (!e2e_name_failure_injected && std::getenv("E2E5_HOST_FAIL_MUSIC_CONTROL_NAME")) {
                    e2e_name_failure_injected = true;
                    throw std::bad_alloc();
                }
#endif
                auto e2e_next_name = Self->Queued->GetFileName();
                if (Self->Current->IsRunning()) Self->Current->RequestStop();''', 1, source.name)
        body = _replace_exact(body, '                Self->CurrentFileName = Self->Current->GetFileName();',
            '                Self->CurrentFileName = std::move(e2e_next_name);', 1, source.name)
        body = _replace_exact(body, '            pas::critical_leave(Self->ControlLock);',
            '''            } catch (...) {
                srhd_awa::platform::e2e5_failure_diagnostics::Write("music control recovered exception");
                srhd_awa::platform::e2e5_failure_diagnostics::Memory("music-control-exception");
                SysUtilsImports::Sleep(1000u);
            }''', 1, source.name)
        text = text[:begin] + body + text[end:]
        start = text.index('    void TMusicUnit_Execute(TMusicUnit* Self) {')
        end = text.index('    void TMusicControl_Create(', start)
        body = text[start:end]
        closing = body.rfind('    }')
        body = body[:closing] + '''        } catch (...) {
            // Opening the decoder can throw before the original try block.
            // Finish this track without killing the worker or stranding waits.
            char e2e_error[256]{};
            try { throw; }
            catch (const std::exception& error) {
                std::snprintf(e2e_error, sizeof(e2e_error),
                    "music worker recovered exception what=%.180s", error.what());
            } catch (...) {
                std::snprintf(e2e_error, sizeof(e2e_error), "music worker recovered exception type=unknown");
            }
            srhd_awa::platform::e2e5_failure_diagnostics::Write(e2e_error);
            srhd_awa::platform::e2e5_failure_diagnostics::Memory("music-unit-exception");
            try { Self->Clear(); } catch (...) {
                try { srhd_awa::platform::e2e_stage::Log("music worker cleanup exception"); } catch (...) {}
            }
            if (Self->CompletionEvent != 0) WindowsSdk::SetEvent(Self->CompletionEvent);
        }
''' + body[closing:]
        body = body.replace('    void TMusicUnit_Execute(TMusicUnit* Self) {',
            '    void TMusicUnit_Execute(TMusicUnit* Self) {\n        try {', 1)
        text = '#include "e2e_stage.hpp"\n#include <cstdio>\n#include <exception>\n#include <cstdlib>\n#include <new>\n' + text[:start] + body + text[end:]
    if e2e5_profile and source.name in ("aGalaxy.cpp", "aShip.cpp", "aMyFunction.cpp", "ab_MainForm.cpp"):
        text = '#include "e2e5_round.hpp"\n' + text.replace('System::Round(',
            'srhd_awa::platform::e2e5_round::Round(')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")
    return destination
