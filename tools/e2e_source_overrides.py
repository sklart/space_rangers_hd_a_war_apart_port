"""Generate narrow Switch source overrides from the pinned, untouched upstream."""

from __future__ import annotations

from pathlib import Path
import re


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
        StoreNativeVorbis(Decoder, opened.decoder);
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
        const auto read = srhd_awa::platform::e2e4_native_vorbis::Read(
            NativeVorbis(Decoder), Buffer, requested);
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
                     manifest_stamp: dict | None = None,
                     audio_on: bool = False, e2e5_profile: bool = False,
                     script_width: bool = False) -> Path:
    overrides = dict(FUNCTIONS.get(source.name, {}))
    if audio_on:
        overrides.update(AUDIO_ON_FUNCTIONS.get(source.name, {}))
    if not overrides and source.name not in ("Rangers.cpp", "Globals.cpp", "GI_MessageLoop.cpp", "GI_GraphButton.cpp", "GR_Main.cpp", "GR_GraphBuf.cpp", "aPirate.cpp", "aPlayer.cpp", "aSaveLoad.cpp", "aScript.cpp", "aShip.cpp", "fGameSettings.cpp", "fGameSettings2.cpp", "fIntroduction.cpp", "fMainForm.cpp", "fRuinsTalk.cpp", "fTalk.cpp", "program.cpp"):
        return source
    text = PROGRAM_SOURCE if source.name == "program.cpp" else source.read_text(encoding="utf-8")
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
            const auto bytes = stream->Read(static_cast<std::uint8_t*>(output) + total,
                static_cast<std::int32_t>(wanted - total));
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
        text = ('#include "e2e4_native_vorbis.hpp"\n#include <algorithm>\n'
                '#include <climits>\n#include <cstring>\n#include <stdexcept>\n' + text)
    if source.name == "GR_Music.cpp":
        marker = "namespace GR_Music {\n"
        if text.count(marker) != 1:
            raise RuntimeError("music namespace boundary changed")
        text = text.replace(marker,
            'namespace VorbisFile { void CloseVorbisStream(TOggWorker* worker); }\n'
            + marker, 1)
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
        text = text[:start] + run + text[end:]
        text = ('#include "e2e5_stage_profiler.hpp"\n' if e2e5_profile else '') + '#include "e2e_stage.hpp"\n' + text
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
    if source.name == "fGameSettings2.cpp":
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
        text = '#include "e2e3_generation_state.hpp"\n' + text
    if source.name == "fIntroduction.cpp":
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
    if source.name == "GR_GraphBuf.cpp":
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
#if defined(E2E4_QUICKSAVE_EXPERIMENT)
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
        if not audio_on:
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
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text(encoding="utf-8") != text:
        destination.write_text(text, encoding="utf-8")
    return destination
