"""Disposable source-tree copy for the experimental native-width script runtime.

Every C++ unit must see the same patched type declarations.  A patched header
beside only one generated .cpp would violate the one-definition rule because
quoted includes in the remaining upstream units resolve inside upstream/src.
"""

from pathlib import Path
from e2e5_ui_width_overrides import apply as apply_ui_width_overrides


def _replace_exact(text: str, old: str, new: str, count: int, path: str) -> str:
    found = text.count(old)
    if found != count:
        raise RuntimeError(f"script-width boundary changed in {path}: {found} != {count}")
    return text.replace(old, new)


def prepare_script_source_overlay(source_root: Path, destination: Path) -> Path:
    source_files: set[Path] = set()
    for source in source_root.rglob("*"):
        if source.is_symlink():
            raise RuntimeError(f"script-width source symlink is unsupported: {source}")
        if not source.is_file():
            continue
        relative = source.relative_to(source_root)
        source_files.add(relative)
        target = destination / relative
        if target.is_symlink():
            raise RuntimeError(f"script-width overlay symlink is unsupported: {target}")
        target.parent.mkdir(parents=True, exist_ok=True)
        content = source.read_bytes()
        if relative.as_posix() in ("types/aScript.hpp", "types/aEFilm.hpp", "aScript.cpp", "aScriptFun.cpp", "aGalaxy.cpp",
                                   "aSaveLoad.cpp", "fSaveManager.cpp",
                                   "types/EC_Expression.hpp", "EC_Expression.cpp",
                                   "units/aPlayer.hpp"):
            text = content.decode("utf-8")
            if relative.as_posix() == "units/aPlayer.hpp":
                text = _replace_exact(text, "extern std::uint32_t EncodedPlayer;",
                                      "extern std::uintptr_t EncodedPlayer;", 1,
                                      relative.as_posix())
            elif relative.as_posix() == "types/aEFilm.hpp":
                # Command views were translated with an 8-byte prefix for
                # two 32-bit links.  On 64-bit both links take 16 bytes.
                text = _replace_exact(text, "        std::uint8_t cpp_padding[8];",
                                      "        std::uint8_t cpp_padding[sizeof(void*) * 2];",
                                      6, relative.as_posix())
                # Endpoints are the widest command payload on 64-bit.
                text = _replace_exact(text,
                    "pas::Array<std::uint8_t, 0, 11> Payload;",
                    "pas::Array<std::uint8_t, 0, sizeof(void*) * 2 + 3> Payload;",
                    1, relative.as_posix())
                text = "#include <cstddef>\n" + text + "\n" + (
                    "static_assert(sizeof(aEFilm::TEFilmCommand) == "
                    "sizeof(aEFilm::TEFilmEndpointsCommand));\n"
                    "static_assert(offsetof(aEFilm::TEFilmCommand, Kind) == "
                    "offsetof(aEFilm::TEFilmEndpointsCommand, Kind));\n"
                    "static_assert(offsetof(aEFilm::TEFilmCommand, Obj) == "
                    "offsetof(aEFilm::TEFilmEndpointsCommand, Obj));\n")
            elif relative.as_posix() == "types/aScript.hpp":
                text = _replace_exact(text,
                    "        std::uint32_t TargetValue;",
                    "        std::uintptr_t TargetValue;", 2, relative.as_posix())
            elif relative.as_posix() == "aScript.cpp":
                text = _replace_exact(text, '#include "layout/aScript.hpp"',
                    '#include "e2e4_script_value.hpp"\n#include "e2e5_save_refs.hpp"\n#include "layout/aScript.hpp"',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "static_cast<std::uint32_t>(TargetValue)",
                    "TargetValue", 6, relative.as_posix())
                text = _replace_exact(text,
                    "Place->TargetValue = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(InitCode->LocalVar->GetVar(Place->TargetVarName)));",
                    "Place->TargetValue = reinterpret_cast<std::uintptr_t>(InitCode->LocalVar->GetVar(Place->TargetVarName));",
                    2, relative.as_posix())
                text = _replace_exact(text,
                    "static_cast<std::uintptr_t>(static_cast<std::uint32_t>(",
                    "static_cast<std::uintptr_t>(static_cast<std::uintptr_t>(",
                    16, relative.as_posix())
                text = _replace_exact(text,
                    "SetDword(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(",
                    "SetDword(static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(",
                    4, relative.as_posix())
                for name, count in (("currentShip", 2), ("star", 2), ("planet", 2),
                                    ("place", 2), ("scriptItem", 1)):
                    text = _replace_exact(text, f"std::uint32_t {name} = "
                        "static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(",
                        f"std::uintptr_t {name} = "
                        "static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(",
                        count, relative.as_posix())
                text = _replace_exact(text,
                    "                Buffer->AddIntegerValue(Cell->GetInt());",
                    "                if (!srhd_awa::platform::e2e4_script_value::FitsDiskInt(Cell->IntValue)) {\n"
                    "                    pas::raise(pas::make_exception<pas::Exception>(static_cast<pas::AnsiString>(pas::concat_wide({u\"E2E4 script pointer cannot be serialized as integer: \", Cell->Name}))));\n"
                    "                }\n"
                    "                Buffer->AddIntegerValue(Cell->GetInt());",
                    2, relative.as_posix())
                save_state_start = text.index("void TScript::SaveState(")
                save_state_end = text.index("void TScript::LoadState(", save_state_start)
                save_state = text[save_state_start:save_state_end]
                save_state = _replace_exact(save_state,
                    "                Buffer->AddWideStringZ(Cell->Name);\n"
                    "                Buffer->AddAnsiChar(static_cast<std::uint8_t>(Kind));\n"
                    "                Buffer->AddDWord(Cell->GetDword());",
                    "                srhd_awa::platform::e2e5_save_refs::WriteScriptDword(Buffer, this, Cell);",
                    2, relative.as_posix())
                save_state = _replace_exact(save_state,
                    "        Ether->SaveToBuffer(Buffer);",
                    "        srhd_awa::platform::e2e5_save_refs::ScriptScope e2e_script_scope(this);\n"
                    "        Ether->SaveToBuffer(Buffer);",
                    1, relative.as_posix())
                text = text[:save_state_start] + save_state + text[save_state_end:]
                load_state_start = text.index("void TScript::LoadState(")
                load_state_end = text.index("void TScript::BindImportedFunctions(", load_state_start)
                load_state = text[load_state_start:load_state_end]
                load_state = _replace_exact(load_state,
                    "            Kind = static_cast<EC_Expression::TVarKind>(EC_Buf::TBufEC_GetByte(Buffer));",
                    "            const std::uint8_t e2e_disk_kind = EC_Buf::TBufEC_GetByte(Buffer);\n"
                    "            if (srhd_awa::platform::e2e5_save_refs::IsObjectKind(e2e_disk_kind) &&\n"
                    "                !srhd_awa::platform::e2e5_save_refs::LoadFormatIsV168())\n"
                    "                pas::raise(pas::make_exception<pas::Exception>(\"E2E5 typed reference in legacy save\"_a));\n"
                    "            Kind = srhd_awa::platform::e2e5_save_refs::IsObjectKind(e2e_disk_kind) ?\n"
                    "                EC_Expression::vkDword : static_cast<EC_Expression::TVarKind>(e2e_disk_kind);",
                    2, relative.as_posix())
                load_state = _replace_exact(load_state,
                    "                    Cell->SetDword(EC_Buf::TBufEC_GetUInt32(Buffer));",
                    "                    const std::uint32_t saved_dword = EC_Buf::TBufEC_GetUInt32(Buffer);\n"
                    "                    if (srhd_awa::platform::e2e5_save_refs::IsObjectKind(e2e_disk_kind)) {\n"
                    "                        if (TemporaryCell) pas::raise(pas::make_exception<pas::Exception>(\"E2E5 typed variable missing from script definition\"_a));\n"
                    "                        Cell->SetDword(saved_dword);\n"
                    "                        srhd_awa::platform::e2e5_save_refs::QueueLoadedPointer(Cell, e2e_disk_kind, saved_dword);\n"
                    "                    } else if (Name == u\"CurShip\" && !TemporaryCell) {\n"
                    "                        CurrentShip = static_cast<aShip::TShip*>(Galaxy->IdToShip(saved_dword, true));\n"
                    "                        Cell->SetDword(reinterpret_cast<std::uintptr_t>(CurrentShip));\n"
                    "                    } else if (Name == u\"temp_planet\" && !TemporaryCell) {\n"
                    "                        Cell->SetDword(reinterpret_cast<std::uintptr_t>(Galaxy->IdToPlanet(saved_dword, true)));\n"
                    "                    } else {\n"
                    "                        Cell->SetDword(saved_dword);\n"
                    "                    }",
                    2, relative.as_posix())
                text = text[:load_state_start] + load_state + text[load_state_end:]
            elif relative.as_posix() == "aGalaxy.cpp":
                text = _replace_exact(text, "namespace aGalaxy {",
                    '#include "e2e5_save_inventory.hpp"\nnamespace aGalaxy {',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "    void TGalaxy::SaveToBuffer(EC_Buf::TBufEC* Buffer) {",
                    "    void TGalaxy::SaveToBuffer(EC_Buf::TBufEC* Buffer) {\n"
                    "        srhd_awa::platform::e2e5_save_refs::SaveScope e2e_save_scope(this);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "        TGalaxy::RunConfigOnSaveHandlers();\n        Buffer->AddWideStringZ(GR_Main::SelectedMods);",
                    "        TGalaxy::RunConfigOnSaveHandlers();\n"
                    "        if (!srhd_awa::platform::e2e5_save_inventory::preflight(this)) {\n"
                    "            pas::raise(pas::make_exception<pas::Exception>(\"E2E5 save inventory contains unknown script values\"_a));\n"
                    "        }\n"
                    "        srhd_awa::platform::e2e5_save_inventory::log_game_state(this, \"save\");\n"
                    "        Buffer->AddWideStringZ(GR_Main::SelectedMods);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "            Stage = 41;\n            Count = pas::list_count(Self->Scripts);",
                    "            Stage = 41;\n"
                    "            srhd_awa::platform::e2e5_save_refs::RebindPending(Self);\n"
                    "            srhd_awa::platform::e2e5_save_inventory::log_game_state(Self, \"load\");\n"
                    "            Count = pas::list_count(Self->Scripts);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "            Script->SaveState(Buffer);",
                    "            EC_Expression::TVarArrayEC* scopes[2] = {Script->InitCode->LocalVar, Script->TurnCode->LocalVar};\n"
                    "            for (std::int32_t scope_index = 0; scope_index < 2; ++scope_index) {\n"
                    "                EC_Expression::TVarArrayEC* scope = scopes[scope_index];\n"
                    "                for (std::int32_t vi = 0; vi < scope->Count; ++vi) {\n"
                    "                    EC_Expression::TVarEC* cell = EC_Expression::TVarArrayEC_GetItem(scope, vi);\n"
                    "                    if (cell->Kind == EC_Expression::vkDword && cell->GetDword() > 0xffffffffu) {\n"
                    "                        GR_Main::AppendLogLineThreadSafe(static_cast<pas::AnsiString>(pas::concat_wide({\n"
                    "                            u\"E2E5 script width inventory: \", Script->ScriptFileName,\n"
                    "                            scope_index == 0 ? u\" init \" : u\" turn \", cell->Name})));\n"
                    "                    }\n"
                    "                }\n"
                    "            }\n"
                    "            Script->SaveState(Buffer);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "        Globals::SharedScriptVariables->SaveToBuffer(Buffer);",
                    "        auto* shared = Globals::SharedScriptVariables;\n"
                    "        Buffer->AddIntegerValue(shared->Count);\n"
                    "        for (std::int32_t vi = 0; vi < shared->Count; ++vi) {\n"
                    "            auto* cell = EC_Expression::TVarArrayEC_GetItem(shared, vi);\n"
                    "            if (cell->Name != u\"GRunStar\") { cell->SaveToBuffer(Buffer); continue; }\n"
                    "            if (cell->Kind != EC_Expression::vkDword)\n"
                    "                pas::raise(pas::make_exception<pas::Exception>(\"E2E5 GRunStar kind changed\"_a));\n"
                    "            const auto binding = srhd_awa::platform::e2e5_save_inventory::classify(this, nullptr, cell);\n"
                    "            if (cell->GetDword() != 0 &&\n"
                    "                (std::string(binding.category) != \"STABLE_OBJECT_ID\" ||\n"
                    "                 std::string(binding.object_type) != \"star\"))\n"
                    "                pas::raise(pas::make_exception<pas::Exception>(\"E2E5 GRunStar is not a galaxy star\"_a));\n"
                    "            Buffer->AddWideStringZ(cell->Name);\n"
                    "            Buffer->AddAnsiChar(static_cast<std::uint8_t>(cell->Kind));\n"
                    "            Buffer->AddDWord(cell->GetDword() == 0 ? 0u : binding.object_id);\n"
                    "        }",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "                if (Existing != nullptr) {\n                    Existing->AssignFrom(Variable, true);\n                }",
                    "                if (Existing != nullptr) {\n"
                    "                    if (Variable->Name == u\"GRunStar\" && Variable->Kind == EC_Expression::vkDword) {\n"
                    "                        const std::uintptr_t saved_id = Variable->GetDword();\n"
                    "                        TStar* rebound = saved_id == 0 ? nullptr : Self->IdToStar(static_cast<std::uint32_t>(saved_id));\n"
                    "                        if (saved_id != 0 && rebound == nullptr) pas::raise(pas::make_exception<pas::Exception>(\"E2E4 GRunStar star ID is invalid\"_a));\n"
                    "                        Variable->SetDword(reinterpret_cast<std::uintptr_t>(rebound));\n"
                    "                    }\n"
                    "                    Existing->AssignFrom(Variable, true);\n"
                    "                }",
                    1, relative.as_posix())
            elif relative.as_posix() == "aSaveLoad.cpp":
                text = _replace_exact(text, '#include "layout/aSaveLoad.hpp"',
                    '#include "e2e5_save_refs.hpp"\n#include "layout/aSaveLoad.hpp"',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    'Header->AddWideStringZ(u"RSG"_wref.get());',
                    'Header->AddWideStringZ(u"RSG64"_wref.get());',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    'Header->AddWideStringZ(static_cast<pas::WideString>(pas::concat_ansi({"v", SysUtils::IntToStr(aConst::CurrentSaveVersion)})));',
                    'Header->AddWideStringZ(u"v168"_wref.get());',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    '            if (F->ReadWideString() != u"RSG") {',
                    '            const pas::WideString e2e_signature = F->ReadWideString();\n'
                    '            if (e2e_signature != u"RSG" && e2e_signature != u"RSG64") {',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    '            GlobalsV::LoadedSaveVersion = EC_Str::ExtractDigitsToIntW(pas::view(F->ReadWideString()));',
                    '            GlobalsV::LoadedSaveVersion = EC_Str::ExtractDigitsToIntW(pas::view(F->ReadWideString()));\n'
                    '            if (e2e_signature == u"RSG64" && GlobalsV::LoadedSaveVersion != 168)\n'
                    '                pas::raise(pas::make_exception<pas::Abort>("E2E5 unsupported typed save version"_a));\n'
                    '            srhd_awa::platform::e2e5_save_refs::SetLoadFormat(e2e_signature == u"RSG64");',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    '        GlobalsV::LoadedSaveVersion = aConst::CurrentSaveVersion;',
                    '        GlobalsV::LoadedSaveVersion = 168;\n'
                    '        srhd_awa::platform::e2e5_save_refs::SetLoadFormat(true);',
                    1, relative.as_posix())
            elif relative.as_posix() == "fSaveManager.cpp":
                text = _replace_exact(text,
                    '                                if (FileObject->ReadWideString() != u"RSG") {',
                    '                                const pas::WideString e2e_signature = FileObject->ReadWideString();\n'
                    '                                if (e2e_signature != u"RSG" && e2e_signature != u"RSG64") {',
                    1, relative.as_posix())
                text = _replace_exact(text,
                    '                                if (I < 13 || I > aConst::CurrentSaveVersion) {',
                    '                                if (I < 13 || I > 168 ||\n'
                    '                                    (e2e_signature == u"RSG64" && I != 168)) {',
                    1, relative.as_posix())
            elif relative.as_posix() == "types/EC_Expression.hpp":
                for old, new in (
                    ("std::uint32_t GetDword();", "std::uintptr_t GetDword();"),
                    ("void SetDword(std::uint32_t Value);", "void SetDword(std::uintptr_t Value);"),
                    ("std::int32_t IntValue;\n        std::uint32_t DwordValue;\n        pas::WideString StringValue;",
                     "std::intptr_t IntValue;\n        std::uintptr_t DwordValue;\n        pas::WideString StringValue;"),
                ):
                    text = _replace_exact(text, old, new, 1, relative.as_posix())
            elif relative.as_posix() == "EC_Expression.cpp":
                text = _replace_exact(text, '#include "layout/EC_Expression.hpp"',
                                      '#include "e2e4_script_value.hpp"\n#include "e2e5_save_refs.hpp"\n#include "layout/EC_Expression.hpp"',
                                      1, relative.as_posix())
                text = _replace_exact(text, "IntValue = DwordValue;",
                    "IntValue = srhd_awa::platform::e2e4_script_value::IntStorageFromDword(DwordValue);",
                    1, relative.as_posix())
                text = _replace_exact(text, "DwordValue = IntValue;",
                    "DwordValue = srhd_awa::platform::e2e4_script_value::DwordFromIntStorage(IntValue);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "std::uint32_t TVarEC::GetDword() {",
                    "std::uintptr_t TVarEC::GetDword() {", 1, relative.as_posix())
                get_start = text.index("std::uintptr_t TVarEC::GetDword() {")
                get_end = text.index("double TVarEC::GetFloat()", get_start)
                get_body = text[get_start:get_end]
                get_body = _replace_exact(get_body, "return IntValue;",
                    "return srhd_awa::platform::e2e4_script_value::DwordFromIntStorage(IntValue);",
                    1, relative.as_posix())
                get_body = _replace_exact(get_body, "return RefValue->GetInt();",
                    "if (pas::is_one_of<vkInt, vkDword, vkString>(TVarEC_RealVType(RefValue))) {\n"
                    "                return RefValue->GetDword();\n"
                    "            }\n"
                    "            return static_cast<std::uint32_t>(RefValue->GetInt());",
                    1, relative.as_posix())
                text = text[:get_start] + get_body + text[get_end:]
                text = _replace_exact(text, "void TVarEC::SetDword(std::uint32_t Value) {",
                    "void TVarEC::SetDword(std::uintptr_t Value) {", 1, relative.as_posix())
                set_start = text.index("void TVarEC::SetDword(std::uintptr_t Value) {")
                set_end = text.index("void TVarEC::SetFloat(", set_start)
                set_body = text[set_start:set_end]
                set_body = _replace_exact(set_body, "IntValue = Value;",
                    "IntValue = srhd_awa::platform::e2e4_script_value::IntStorageFromDword(Value);",
                    1, relative.as_posix())
                set_body = _replace_exact(set_body, "RefValue->SetInt(Value);",
                    "if (pas::is_one_of<vkInt, vkDword>(TVarEC_RealVType(RefValue)) ||\n"
                    "                        !srhd_awa::platform::e2e4_script_value::FitsDiskDword(Value)) {\n"
                    "                        RefValue->SetDword(Value);\n"
                    "                    } else {\n"
                    "                        RefValue->SetInt(static_cast<std::int32_t>(Value));\n"
                    "                    }",
                    1, relative.as_posix())
                text = text[:set_start] + set_body + text[set_end:]
                compare_start = text.index("void TVarEC_OEqual(")
                compare_end = text.index("void TVarEC::SaveToBuffer(", compare_start)
                compare_body = text[compare_start:compare_end]
                compare_body = _replace_exact(compare_body,
                    "std::uint32_t cpp_left_2 = Left->GetDword();",
                    "std::uintptr_t cpp_left_2 = Left->GetDword();", 6, relative.as_posix())
                compare_body = _replace_exact(compare_body,
                    "std::uint32_t cpp_left_2 = Self->GetDword();",
                    "std::uintptr_t cpp_left_2 = Self->GetDword();", 3, relative.as_posix())
                text = text[:compare_start] + compare_body + text[compare_end:]
                text = _replace_exact(text,
                    "                Buffer->AddIntegerValue(IntValue);",
                    "                if (!srhd_awa::platform::e2e4_script_value::FitsDiskInt(IntValue)) {\n"
                    "                    pas::raise(pas::make_exception<ExceptionExpressionEC>(static_cast<pas::AnsiString>(pas::concat_wide({u\"E2E4 script pointer cannot be serialized as integer: \", Name}))));\n"
                    "                }\n"
                    "                Buffer->AddIntegerValue(IntValue);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "                Buffer->AddDWord(DwordValue);",
                    "                Buffer->AddDWord(srhd_awa::platform::e2e5_save_refs::NestedDiskValue(this));",
                    1, relative.as_posix())
                save_start = text.index("void TVarEC::SaveToBuffer(")
                save_end = text.index("void TVarEC::LoadFromBuffer(", save_start)
                save_body = text[save_start:save_end]
                save_body = _replace_exact(save_body,
                    "        Buffer->AddAnsiChar(static_cast<std::uint8_t>(Kind));",
                    "        Buffer->AddAnsiChar(Kind == vkDword ?\n"
                    "            srhd_awa::platform::e2e5_save_refs::NestedDiskKind(this) :\n"
                    "            static_cast<std::uint8_t>(Kind));",
                    1, relative.as_posix())
                text = text[:save_start] + save_body + text[save_end:]
                load_start = text.index("void TVarEC::LoadFromBuffer(")
                load_end = text.index("void TVarArrayEC_Create(", load_start)
                load_body = text[load_start:load_end]
                load_body = _replace_exact(load_body,
                    "        ResetKind(static_cast<TVarKind>(EC_Buf::TBufEC_GetByte(Buffer)));",
                    "        const std::uint8_t e2e_disk_kind = EC_Buf::TBufEC_GetByte(Buffer);\n"
                    "        if (srhd_awa::platform::e2e5_save_refs::IsObjectKind(e2e_disk_kind) &&\n"
                    "            !srhd_awa::platform::e2e5_save_refs::LoadFormatIsV168())\n"
                    "            pas::raise(pas::make_exception<ExceptionExpressionEC>(\"E2E5 typed reference in legacy save\"_a));\n"
                    "        ResetKind(srhd_awa::platform::e2e5_save_refs::IsObjectKind(e2e_disk_kind) ?\n"
                    "            vkDword : static_cast<TVarKind>(e2e_disk_kind));",
                    1, relative.as_posix())
                load_body = _replace_exact(load_body,
                    "                DwordValue = EC_Buf::TBufEC_GetUInt32(Buffer);",
                    "                DwordValue = EC_Buf::TBufEC_GetUInt32(Buffer);\n"
                    "                if (srhd_awa::platform::e2e5_save_refs::IsObjectKind(e2e_disk_kind))\n"
                    "                    srhd_awa::platform::e2e5_save_refs::QueueLoadedPointer(this, e2e_disk_kind, static_cast<std::uint32_t>(DwordValue));",
                    1, relative.as_posix())
                text = text[:load_start] + load_body + text[load_end:]
            else:
                text = _replace_exact(text,
                    "static_cast<std::uint32_t>(Place->TargetValue)",
                    "Place->TargetValue", 1, relative.as_posix())
                for name in ("storedItem", "requireCustomWeaponInfo"):
                    text = _replace_exact(text, f"std::uint32_t {name} = "
                        "static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(",
                        f"std::uintptr_t {name} = "
                        "static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(",
                        1, relative.as_posix())
                text = _replace_exact(text,
                    "if (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Obj)) < 0x00010000)",
                    "if (reinterpret_cast<std::uintptr_t>(Obj) < 0x00010000)",
                    2, relative.as_posix())
                text = _replace_exact(text,
                    "static_cast<std::uintptr_t>(static_cast<std::uint32_t>(",
                    "static_cast<std::uintptr_t>(static_cast<std::uintptr_t>(",
                    560, relative.as_posix())
                text = _replace_exact(text,
                    "SetDword(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(",
                    "SetDword(static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(",
                    137, relative.as_posix())
            content = text.encode("utf-8")
        if relative.as_posix() == "types/GI_MessageLoop.hpp":
            content = apply_ui_width_overrides(relative.as_posix(), content.decode("utf-8")).encode("utf-8")
        if not target.is_file() or target.read_bytes() != content:
            target.write_bytes(content)
    stale = [p.relative_to(destination) for p in destination.rglob("*")
             if p.is_file() and p.relative_to(destination) not in source_files]
    if stale:
        raise RuntimeError(f"stale files in script-width source overlay: {stale[:5]}")
    return destination
