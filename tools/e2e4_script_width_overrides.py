"""Disposable source-tree copy for the experimental native-width script runtime.

Every C++ unit must see the same patched type declarations.  A patched header
beside only one generated .cpp would violate the one-definition rule because
quoted includes in the remaining upstream units resolve inside upstream/src.
"""

from pathlib import Path


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
        if relative.as_posix() in ("types/aScript.hpp", "aScript.cpp", "aScriptFun.cpp"):
            text = content.decode("utf-8")
            if relative.as_posix() == "types/aScript.hpp":
                text = _replace_exact(text,
                    "        std::uint32_t TargetValue;",
                    "        std::uintptr_t TargetValue;", 2, relative.as_posix())
            elif relative.as_posix() == "aScript.cpp":
                text = _replace_exact(text,
                    "static_cast<std::uint32_t>(TargetValue)",
                    "TargetValue", 6, relative.as_posix())
                text = _replace_exact(text,
                    "Place->TargetValue = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(InitCode->LocalVar->GetVar(Place->TargetVarName)));",
                    "Place->TargetValue = reinterpret_cast<std::uintptr_t>(InitCode->LocalVar->GetVar(Place->TargetVarName));",
                    2, relative.as_posix())
            else:
                text = _replace_exact(text,
                    "static_cast<std::uint32_t>(Place->TargetValue)",
                    "Place->TargetValue", 1, relative.as_posix())
            content = text.encode("utf-8")
        if not target.is_file() or target.read_bytes() != content:
            target.write_bytes(content)
    stale = [p.relative_to(destination) for p in destination.rglob("*")
             if p.is_file() and p.relative_to(destination) not in source_files]
    if stale:
        raise RuntimeError(f"stale files in script-width source overlay: {stale[:5]}")
    return destination
