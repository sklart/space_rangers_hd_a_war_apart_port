"""Inventory static Win32 imports and dynamic module expressions in E2E sources."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re

from e2e_source_overrides import FUNCTIONS

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "upstream/cpp"
BUILD = ROOT / "port/switch/build/e2e-game"
CLASSIFICATIONS = ROOT / "tools/win32_surface_classifications.json"
KINDS = {
    "PORTABLE", "OPTIONAL_DISABLED", "STATIC_LIBRARY", "RUNTIME_MODULE",
    "REQUIRED_UNIMPLEMENTED", "NOT_REACHED_UNTIL_GAMEPLAY", "UNKNOWN",
}

IMPORT = re.compile(
    r"(?:(?:pas::)?win::)?(load_import<[^>\n]+>|resolve_import)\s*\(\s*"
    r'"([^"\n]+)"\s*,\s*(?:"([^"\n]+)"|(\d+))', re.I)
DLL_LITERAL = re.compile(r'"([^"\n]*?\.dll)"', re.I)
DIRECT_DECLARATION = re.compile(r"__declspec\(dllimport\)([^;]+);", re.S)
DYNAMIC = re.compile(
    r"\b(?:WindowsImports|WindowsSdk)::(?:LoadLibrary(?:W)?|GetProcAddress|GetModuleHandle(?:W)?)\s*\(")
LOAD_IMPORT_TOKEN = re.compile(r"\b(?:load_import|resolve_import)\s*(?:<|\()")


def source_line(content: str, offset: int) -> int:
    return content.count("\n", 0, offset) + 1


def effective_switch_text(content: str) -> str:
    """Blank known inactive platform branches without changing line numbers."""
    frames: list[tuple[bool, bool | None]] = []
    active = True
    output: list[str] = []
    for line in content.splitlines(keepends=True):
        stripped = line.strip()
        condition: bool | None = None
        if stripped.startswith("#ifdef ") or stripped.startswith("#ifndef "):
            parts = stripped.split()
            if len(parts) >= 2 and parts[1] in {"_WIN32", "__SWITCH__"}:
                condition = parts[1] == "__SWITCH__"
                if parts[0] == "#ifndef":
                    condition = not condition
        elif stripped.startswith("#if ") or stripped.startswith("#elif "):
            expression = stripped.split(None, 1)[1].strip()
            known = {"defined(__SWITCH__)": True,
                     "defined(_WIN32)": False,
                     "!defined(__SWITCH__)": False,
                     "!defined(_WIN32)": True}
            condition = known.get(expression)
        if stripped.startswith(("#if ", "#ifdef ", "#ifndef ")):
            frames.append((active, condition))
            active = active and condition is not False
        elif stripped.startswith("#else") and frames:
            parent, previous = frames[-1]
            active = parent and previous is not True
        elif stripped.startswith("#elif ") and frames:
            parent, previous = frames[-1]
            active = parent and previous is not True and condition is not False
            frames[-1] = (parent, previous if previous is True else condition)
        elif stripped.startswith("#endif") and frames:
            active = frames.pop()[0]
        output.append(line if active else "".join("\n" if char == "\n" else " " for char in line))
    return "".join(output)


def args_from(content: str, open_paren: int) -> str:
    """Return the balanced argument list for a call starting at `(`."""
    depth = 0
    quoted = False
    escaped = False
    for index in range(open_paren, len(content)):
        char = content[index]
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return content[open_paren + 1:index].strip()
    raise ValueError("unbalanced C++ call")


def selected_sources(effective: bool) -> list[Path]:
    paths = sorted((*((CPP / "src").rglob("*.cpp")),
                    *((CPP / "src").rglob("*.hpp")),
                    *((CPP / "runtime").rglob("*.cpp")),
                    *((CPP / "runtime").rglob("*.hpp"))))
    if not effective:
        return paths
    selected: list[Path] = []
    generated = BUILD / "generated"
    patched = BUILD / "patched-upstream/src"
    overlay = BUILD / "runtime-overlay"
    for path in paths:
        replacement: Path | None = None
        if path.parent == CPP / "src" and path.suffix == ".cpp":
            candidate = generated / path.name
            generated_names = {"Rangers.cpp", "Globals.cpp", "GR_Main.cpp",
                               "aSaveLoad.cpp", "program.cpp"}
            if candidate.exists() and (path.name in FUNCTIONS or path.name in generated_names):
                replacement = candidate
            elif (patched / path.name).exists():
                replacement = patched / path.name
        elif path.parent == CPP / "runtime":
            candidate = overlay / path.name
            if candidate.exists():
                replacement = candidate
        selected.append(replacement or path)
    selected.extend(sorted((ROOT / "port/switch/platform").glob("*.cpp")))
    selected.extend(sorted((ROOT / "port/switch/platform").glob("*.hpp")))
    return selected


def scan_text(name: str, content: str, entries: dict,
              dynamic: list, literals: dict, unparsed: list,
              direct: list | None = None) -> None:
    parsed_offsets: set[int] = set()
    for match in IMPORT.finditer(content):
        dll = match.group(2).lower()
        symbol = match.group(3) or "#" + match.group(4)
        key = (dll, symbol)
        entries[key].add(f"{name}:{source_line(content, match.start())}")
        parsed_offsets.add(match.start(1))
    for match in LOAD_IMPORT_TOKEN.finditer(content):
        # Generic template definitions forward variable parameters and are
        # recorded separately; every other unparsed call needs review.
        if match.start() in parsed_offsets:
            continue
        line = content[content.rfind("\n", 0, match.start()) + 1:
                       content.find("\n", match.start()) if "\n" in content[match.start():] else len(content)]
        if ("Function load_import(" in line or "return load_import<Function>(" in line
                or "resolve_import(library, name)" in line):
            continue
        if "resolve_import(const char *library" in line:
            continue
        unparsed.append({"callsite": f"{name}:{source_line(content, match.start())}",
                         "expression": line.strip(), "classification": "UNKNOWN"})
    for match in DYNAMIC.finditer(content):
        expression = args_from(content, content.index("(", match.start()))
        dynamic.append({
            "api": match.group(0).split("::")[-1].split("(")[0].strip(),
            "expression": expression,
            "callsite": f"{name}:{source_line(content, match.start())}",
            "expression_kind": ("LITERAL" if re.search(r'"[^"\n]+\.dll"', expression, re.I)
                                else "DYNAMIC_MODULE_EXPRESSION"),
            "classification": "UNKNOWN",
        })
    for match in DLL_LITERAL.finditer(content):
        literals[match.group(1).lower()].add(
            f"{name}:{source_line(content, match.start())}")
    if direct is not None:
        for match in DIRECT_DECLARATION.finditer(content):
            functions = re.findall(r"\b([A-Za-z_]\w*)\s*\(", match.group(1))
            if len(functions) != 1:
                unparsed.append({"callsite": f"{name}:{source_line(content, match.start())}",
                                 "expression": match.group(0).strip(),
                                 "classification": "UNKNOWN"})
                continue
            symbol = functions[0]
            dll = ("oleaut32.dll" if symbol.startswith("Sys") else
                   "winmm.dll" if symbol.startswith("time") else "kernel32.dll")
            direct.append({"dll": dll, "symbol": symbol,
                           "callsite": f"{name}:{source_line(content, match.start())}",
                           "classification": "UNKNOWN"})


def scan(effective: bool, classification: dict | None = None) -> dict:
    classification = classification or {}
    entries: dict[tuple[str, str], set[str]] = defaultdict(set)
    literals: dict[str, set[str]] = defaultdict(set)
    dynamic: list[dict] = []
    direct: list[dict] = []
    unparsed: list[dict] = []
    sources = selected_sources(effective)
    for path in sources:
        content = path.read_text(encoding="utf-8")
        if effective:
            content = effective_switch_text(content)
        scan_text(path.relative_to(ROOT).as_posix(), content,
                  entries, dynamic, literals, unparsed, direct)
    imports = []
    for (dll, symbol), callsites in sorted(entries.items()):
        data = classification.get("imports", {}).get(dll + "::" + symbol, {})
        kind = data.get("classification", "UNKNOWN")
        if kind not in KINDS:
            raise ValueError(f"invalid classification: {dll}::{symbol}: {kind}")
        imports.append({
            "dll": dll, "symbol": symbol, "callsites": sorted(callsites),
            "classification": kind, "implementation": data.get("implementation"),
            "startup_required": data.get("startup_required", False),
            "gameplay_required": data.get("gameplay_required", False),
            "reason": data.get("reason", ""),
        })
    dynamic_class = classification.get("dynamic", {})
    for item in dynamic:
        key = Path(item["callsite"].split(":")[0]).name + "::" + item["api"] + "::" + item["expression"]
        item.update(dynamic_class.get(key, {}))
        if item["classification"] not in KINDS:
            raise ValueError(f"invalid dynamic classification: {key}")
    counts = Counter(item["classification"] for item in imports)
    counts.update(item["classification"] for item in dynamic)
    for item in direct:
        item.update(classification.get("direct", {}).get(item["dll"] + "::" +
                                                    item["symbol"], {}))
        if item["classification"] not in KINDS:
            raise ValueError(f"invalid direct declaration: {item}")
    counts.update(item["classification"] for item in direct)
    counts.update(item["classification"] for item in unparsed)
    return {
        "source": "effective-e2e" if effective else "pinned-upstream",
        "upstream_pin": "57fa689c630193a66fdea6ca4c79a188814991cd",
        "source_file_count": len(sources),
        "imports": imports,
        "dynamic_calls": sorted(dynamic, key=lambda item: item["callsite"]),
        "direct_declarations": sorted(direct, key=lambda item: item["callsite"]),
        "dll_literals": [{"dll": dll, "callsites": sorted(callsites)}
                         for dll, callsites in sorted(literals.items())],
        "unparsed_import_patterns": unparsed,
        "classification_counts": dict(sorted(counts.items())),
        "startup_unimplemented": sum(item.get("startup_required", False) and
            item["classification"] not in {"PORTABLE", "STATIC_LIBRARY"}
            for item in [*imports, *dynamic, *direct, *unparsed]),
    }


def self_test() -> None:
    entries: dict = defaultdict(set)
    dynamic: list = []
    literals: dict = defaultdict(set)
    unparsed: list = []
    direct: list = []
    sample = '''auto a = pas::win::load_import<Fn>("foo.dll", "A");
auto b = win::resolve_import("foo.dll", "B");
auto c = pas::win::load_import<Fn>("foo.dll", 42);
auto d = WindowsImports::LoadLibrary(pas::literal_pointer("bar.dll"));
auto e = WindowsImports::LoadLibrary(variable);
auto f = WindowsImports::GetProcAddress(handle, name);
__declspec(dllimport) void *__stdcall HeapAlloc(void *, unsigned long, unsigned long);
'''
    scan_text("sample.cpp", sample, entries, dynamic, literals, unparsed, direct)
    assert set(entries) == {("foo.dll", "A"), ("foo.dll", "B"),
                            ("foo.dll", "#42")}
    assert len(dynamic) == 3 and dynamic[1]["expression_kind"] == "DYNAMIC_MODULE_EXPRESSION"
    assert "bar.dll" in literals and not unparsed
    assert [(item["dll"], item["symbol"]) for item in direct] == [
        ("kernel32.dll", "HeapAlloc")]
    conditional = '#ifdef _WIN32\nload_import<Fn>("old.dll", "Old");\n#else\nload_import<Fn>("new.dll", "New");\n#endif\n'
    filtered = effective_switch_text(conditional)
    assert 'old.dll' not in filtered and 'new.dll' in filtered
    assert filtered.count("\n") == conditional.count("\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--effective", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("win32 surface scanner self-test PASS")
        return 0
    classification = json.loads(CLASSIFICATIONS.read_text(encoding="utf-8")) if CLASSIFICATIONS.exists() else {}
    result = scan(args.effective, classification)
    output = ROOT / ("win32-effective-import-manifest.json" if args.effective
                     else "win32-import-manifest.json")
    rendered = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    stale = not output.exists() or output.read_text(encoding="utf-8") != rendered
    if stale and not args.check:
        output.write_text(rendered, encoding="utf-8")
    print(f"{result['source']}: {len(result['imports'])} imports, "
          f"{len(result['dynamic_calls'])} dynamic calls, "
          f"{len(result['unparsed_import_patterns'])} unparsed, "
          f"UNKNOWN={result['classification_counts'].get('UNKNOWN', 0)}; "
          f"SHA-256={hashlib.sha256(rendered.encode()).hexdigest().upper()}")
    if args.check and stale:
        print(f"stale Win32 manifest: {output.name}")
    return 1 if args.check and (stale or result["classification_counts"].get("UNKNOWN", 0)
                               or result["startup_unimplemented"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
