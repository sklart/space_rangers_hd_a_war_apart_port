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
                    '#include "e2e4_script_value.hpp"\n#include "layout/aScript.hpp"',
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
                save_state = save_state.replace(
                    "                Buffer->AddDWord(Cell->GetDword());",
                    "                bool rebound_binding = false;\n"
                    "                for (std::int32_t si = 0; si < pas::list_count(Stars); ++si) {\n"
                    "                        TScriptStar* bound = pas::list_at<TScriptStar>(Stars, si);\n"
                    "                        if (bound->Name == Cell->Name && bound->Star != nullptr &&\n"
                    "                            Cell->GetDword() == reinterpret_cast<std::uintptr_t>(bound->Star)) {\n"
                    "                            rebound_binding = true;\n"
                    "                            break;\n"
                    "                        }\n"
                    "                        if (bound->Planets != nullptr) {\n"
                    "                            for (std::int32_t pi = 0; pi < bound->Planets.length(); ++pi) {\n"
                    "                                const auto& planet = bound->Planets[pi];\n"
                    "                                if (planet.Name == Cell->Name && planet.Planet != nullptr &&\n"
                    "                                    Cell->GetDword() == reinterpret_cast<std::uintptr_t>(planet.Planet)) {\n"
                    "                                    rebound_binding = true;\n"
                    "                                    break;\n"
                    "                                }\n"
                    "                            }\n"
                    "                            if (rebound_binding) break;\n"
                    "                        }\n"
                    "                }\n"
                    "                if (!rebound_binding) {\n"
                    "                    for (std::int32_t pi = 0; pi < pas::list_count(Places); ++pi) {\n"
                    "                        TScriptPlace* bound = pas::list_at<TScriptPlace>(Places, pi);\n"
                    "                        if (bound->Name == Cell->Name &&\n"
                    "                            Cell->GetDword() == reinterpret_cast<std::uintptr_t>(bound)) {\n"
                    "                            rebound_binding = true;\n"
                    "                            break;\n"
                    "                        }\n"
                    "                    }\n"
                    "                }\n"
                    "                if (!rebound_binding) {\n"
                    "                    for (std::int32_t ii = 0; ii < pas::list_count(Items); ++ii) {\n"
                    "                        TScriptItem* bound = pas::list_at<TScriptItem>(Items, ii);\n"
                    "                        if (bound->Name == Cell->Name &&\n"
                    "                            Cell->GetDword() == reinterpret_cast<std::uintptr_t>(bound)) {\n"
                    "                            rebound_binding = true;\n"
                    "                            break;\n"
                    "                        }\n"
                    "                    }\n"
                    "                }\n"
                    "                const bool rebound_ship = Cell->Name == u\"CurShip\" && CurrentShip != nullptr &&\n"
                    "                    Cell->GetDword() == reinterpret_cast<std::uintptr_t>(CurrentShip);\n"
                    "                std::uint32_t rebound_planet_id = 0;\n"
                    "                bool rebound_planet = false;\n"
                    "                if (Cell->Name == u\"temp_planet\" && aGalaxy::Galaxy != nullptr) {\n"
                    "                    for (std::int32_t pi = 0; pi < pas::list_count(aGalaxy::Galaxy->Planets); ++pi) {\n"
                    "                        aPlanet::TPlanet* planet = pas::list_at<aPlanet::TPlanet>(aGalaxy::Galaxy->Planets, pi);\n"
                    "                        if (Cell->GetDword() == reinterpret_cast<std::uintptr_t>(planet)) {\n"
                    "                            rebound_planet_id = planet->Id;\n"
                    "                            rebound_planet = true;\n"
                    "                            break;\n"
                    "                        }\n"
                    "                    }\n"
                    "                    if (!rebound_planet) {\n"
                    "                        for (std::int32_t si = 0; si < pas::list_count(aGalaxy::Galaxy->Stars); ++si) {\n"
                    "                            aGalaxy::TStar* star = pas::list_at<aGalaxy::TStar>(aGalaxy::Galaxy->Stars, si);\n"
                    "                            for (std::int32_t pi = 0; pi < pas::list_count(star->Planets); ++pi) {\n"
                    "                                aPlanet::TPlanet* planet = pas::list_at<aPlanet::TPlanet>(star->Planets, pi);\n"
                    "                                if (Cell->GetDword() == reinterpret_cast<std::uintptr_t>(planet)) {\n"
                    "                                    rebound_planet_id = planet->Id;\n"
                    "                                    rebound_planet = true;\n"
                    "                                    break;\n"
                    "                                }\n"
                    "                            }\n"
                    "                            if (rebound_planet) break;\n"
                    "                        }\n"
                    "                    }\n"
                    "                }\n"
                    "                if (!rebound_binding && !rebound_ship && !rebound_planet &&\n"
                    "                    !srhd_awa::platform::e2e4_script_value::FitsDiskDword(Cell->GetDword())) {\n"
                    "                    pas::raise(pas::make_exception<pas::Exception>(static_cast<pas::AnsiString>(pas::concat_wide({u\"E2E4 script pointer cannot be serialized as DWORD: \", Cell->Name}))));\n"
                    "                }\n"
                    "                Buffer->AddDWord(rebound_binding ? 0u :\n"
                    "                    (rebound_ship ? CurrentShip->Id :\n"
                    "                    (rebound_planet ? rebound_planet_id : Cell->GetDword())));",
                    1)
                text = text[:save_state_start] + save_state + text[save_state_end:]
                load_state_start = text.index("void TScript::LoadState(")
                load_state = text[load_state_start:]
                load_state = load_state.replace(
                    "                    Cell->SetDword(EC_Buf::TBufEC_GetUInt32(Buffer));",
                    "                    const std::uint32_t saved_dword = EC_Buf::TBufEC_GetUInt32(Buffer);\n"
                    "                    if (Name == u\"CurShip\" && !TemporaryCell) {\n"
                    "                        CurrentShip = static_cast<aShip::TShip*>(Galaxy->IdToShip(saved_dword, true));\n"
                    "                        Cell->SetDword(reinterpret_cast<std::uintptr_t>(CurrentShip));\n"
                    "                    } else if (Name == u\"temp_planet\" && !TemporaryCell) {\n"
                    "                        Cell->SetDword(reinterpret_cast<std::uintptr_t>(Galaxy->IdToPlanet(saved_dword, true)));\n"
                    "                    } else {\n"
                    "                        Cell->SetDword(saved_dword);\n"
                    "                    }",
                    1)
                text = text[:load_state_start] + load_state
                text = _replace_exact(text,
                    "                Buffer->AddDWord(Cell->GetDword());",
                    "                if (!srhd_awa::platform::e2e4_script_value::FitsDiskDword(Cell->GetDword())) {\n"
                    "                    pas::raise(pas::make_exception<pas::Exception>(static_cast<pas::AnsiString>(pas::concat_wide({u\"E2E4 script pointer cannot be serialized as DWORD: \", Cell->Name}))));\n"
                    "                }\n"
                    "                Buffer->AddDWord(Cell->GetDword());",
                    1, relative.as_posix())
            elif relative.as_posix() == "aGalaxy.cpp":
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
                    "        EC_Expression::TVarEC* run_star = Globals::SharedScriptVariables->GetVar(u\"GRunStar\"_wref.get());\n"
                    "        const std::uintptr_t run_star_address = run_star->GetDword();\n"
                    "        std::uint32_t run_star_id = 0;\n"
                    "        if (run_star_address != 0) {\n"
                    "            bool found = false;\n"
                    "            for (std::int32_t si = 0; si < pas::list_count(Stars); ++si) {\n"
                    "                TStar* candidate = pas::list_at<TStar>(Stars, si);\n"
                    "                if (reinterpret_cast<std::uintptr_t>(candidate) == run_star_address) {\n"
                    "                    run_star_id = candidate->Id;\n"
                    "                    found = true;\n"
                    "                    break;\n"
                    "                }\n"
                    "            }\n"
                    "            if (!found) pas::raise(pas::make_exception<pas::Exception>(\"E2E4 GRunStar does not refer to a galaxy star\"_a));\n"
                    "        }\n"
                    "        run_star->SetDword(run_star_id);\n"
                    "        try { Globals::SharedScriptVariables->SaveToBuffer(Buffer); }\n"
                    "        catch (...) { run_star->SetDword(run_star_address); throw; }\n"
                    "        run_star->SetDword(run_star_address);",
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
                                      '#include "e2e4_script_value.hpp"\n#include "layout/EC_Expression.hpp"',
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
                    "                if (!srhd_awa::platform::e2e4_script_value::FitsDiskDword(DwordValue)) {\n"
                    "                    pas::raise(pas::make_exception<ExceptionExpressionEC>(static_cast<pas::AnsiString>(pas::concat_wide({u\"E2E4 script pointer cannot be serialized as DWORD: \", Name}))));\n"
                    "                }\n"
                    "                Buffer->AddDWord(DwordValue);",
                    1, relative.as_posix())
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
