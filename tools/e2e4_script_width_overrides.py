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
        if relative.as_posix() in ("types/aScript.hpp", "aScript.cpp", "aScriptFun.cpp",
                                   "types/EC_Expression.hpp", "EC_Expression.cpp"):
            text = content.decode("utf-8")
            if relative.as_posix() == "types/aScript.hpp":
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
                    "                    pas::raise(pas::make_exception<pas::Exception>(\"E2E4 script pointer cannot be serialized as integer\"_a));\n"
                    "                }\n"
                    "                Buffer->AddIntegerValue(Cell->GetInt());",
                    2, relative.as_posix())
                text = _replace_exact(text,
                    "                Buffer->AddDWord(Cell->GetDword());",
                    "                if (!srhd_awa::platform::e2e4_script_value::FitsDiskDword(Cell->GetDword())) {\n"
                    "                    pas::raise(pas::make_exception<pas::Exception>(\"E2E4 script pointer cannot be serialized as DWORD\"_a));\n"
                    "                }\n"
                    "                Buffer->AddDWord(Cell->GetDword());",
                    2, relative.as_posix())
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
                    "                    pas::raise(pas::make_exception<ExceptionExpressionEC>(\"E2E4 script pointer cannot be serialized as integer\"_a));\n"
                    "                }\n"
                    "                Buffer->AddIntegerValue(IntValue);",
                    1, relative.as_posix())
                text = _replace_exact(text,
                    "                Buffer->AddDWord(DwordValue);",
                    "                if (!srhd_awa::platform::e2e4_script_value::FitsDiskDword(DwordValue)) {\n"
                    "                    pas::raise(pas::make_exception<ExceptionExpressionEC>(\"E2E4 script pointer cannot be serialized as DWORD\"_a));\n"
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
