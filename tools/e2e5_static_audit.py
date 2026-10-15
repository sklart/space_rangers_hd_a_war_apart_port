"""Inventory potentially lossy pointer conversions in original and Switch code.

This is a source inventory, not a C++ type checker.  Existing unresolved sites
are kept in a reviewed baseline; --check rejects newly introduced sites.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = (ROOT / "upstream/cpp/src", ROOT / "upstream/cpp/runtime", ROOT / "port/switch/platform")
PATTERNS = {
    "ui_payload": re.compile(r"\b(?:UserIndex|UserValue)\b"),
    "pointer_to_32_bit": re.compile(r"(?:reinterpret_cast|static_cast)\s*<\s*(?:std::)?(?:u?int32_t|DWORD|LONG|int)\s*>\s*\([^;]*(?:reinterpret_cast\s*<|\b(?:UserIndex|UserValue|TargetValue)\b)"),
    "32_bit_to_pointer": re.compile(r"reinterpret_cast\s*<[^>]*\*\s*>\s*\([^;]*(?:GetDword\s*\(|UserIndex|UserValue|TargetValue|static_cast\s*<\s*(?:std::)?(?:u?int32_t|int32_t))"),
    "pointer_narrowing_chain": re.compile(r"(?:static_cast\s*<\s*(?:std::)?(?:u?int32_t|int32_t)|\(\s*(?:std::)?(?:u?int32_t|int32_t)\s*\))[^;]*reinterpret_cast"),
    "four_byte_pointer_assumption": re.compile(r"(?:sizeof\s*\(\s*(?:void\s*\*|uintptr_t)\s*\)\s*==\s*4|(?:VMT|vmt|VirtualMethodTable)[^;]*(?:\+\s*4|sizeof\s*\(\s*(?:std::)?uint32_t))"),
}
EXCLUDE = {"build", ".git", ".worktrees"}


def classify(path: str, line: str, kind: str) -> tuple[str, str]:
    """Only classify conversions whose semantics are established by context."""
    if "static_assert" in line or "//" in line.split("reinterpret_cast", 1)[0]:
        return "non_executable", "declaration or comment"
    if kind == "ui_payload":
        if re.search(r"\b(?:UserIndex|UserValue)\s*=\s*(?:static_cast\s*<[^>]+>\s*\(\s*)?reinterpret_cast", line):
            return "confirmed_pointer_narrowing", "UI field assigned from a live pointer; inspect disposable override and every reader"
        if re.search(r"\b(?:UserIndex|UserValue)\s*=\s*-?\d+\s*;", line):
            return "numeric_literal", "literal UI payload"
        return "needs_review", "UI payload semantics require producer/consumer classification"
    if "aGalaxy.cpp" in path and "IdTo" in line:
        return "serialized_id", "explicit galaxy ID conversion; disk width stays 32 bit"
    if "aScript" in path or "EC_Expression" in path:
        return "needs_review", "script value may hold a live pointer or a numeric ID"
    if "UserIndex" in line or "UserValue" in line:
        return "needs_review", "UI payload requires producer and consumer review"
    if kind == "four_byte_pointer_assumption":
        return "needs_review", "layout and VMT width require ABI review"
    return "needs_review", "pointer provenance requires manual inspection"


def inventory() -> dict:
    rows = []
    modules = 0
    for source_root in SOURCE_ROOTS:
        for path in sorted(source_root.rglob("*")):
            if not path.is_file() or path.suffix not in {".cpp", ".hpp", ".h"} or any(part in EXCLUDE for part in path.relative_to(source_root).parts):
                continue
            modules += 1
            rel = path.relative_to(ROOT).as_posix()
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                for kind, pattern in PATTERNS.items():
                    if not pattern.search(line):
                        continue
                    normalized = " ".join(line.strip().split())
                    key = hashlib.sha256(f"{rel}|{kind}|{normalized}".encode()).hexdigest()[:20]
                    category, reason = classify(rel, normalized, kind)
                    rows.append({"id": key, "file": rel, "line": number, "kind": kind,
                                 "classification": category, "reason": reason, "source": normalized[:300]})
    rows.sort(key=lambda row: (row["file"], row["line"], row["kind"]))
    return {"schema": 1, "level": "STATIC SOURCE INVENTORY", "modules_scanned": modules,
            "findings": rows, "finding_count": len(rows),
            "unresolved_count": sum(row["classification"] == "needs_review" for row in rows),
            "confirmed_pointer_narrowing_count": sum(row["classification"] == "confirmed_pointer_narrowing" for row in rows),
            "ui_payload_sites": sum(row["kind"] == "ui_payload" for row in rows)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    parser.add_argument("--baseline", type=Path, default=ROOT / "reports/e2e5/pointer-width-baseline.json")
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    result = inventory()
    if args.write_baseline:
        args.baseline.parent.mkdir(parents=True, exist_ok=True)
        args.baseline.write_text(json.dumps({"schema": 1, "known_ids": sorted({r["id"] for r in result["findings"]})}, indent=2) + "\n", encoding="utf-8")
    if args.check:
        known = set(json.loads(args.baseline.read_text(encoding="utf-8"))["known_ids"])
        result["new_unclassified"] = [r for r in result["findings"] if r["id"] not in known and
                                      r["classification"] in {"needs_review", "confirmed_pointer_narrowing"}]
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k != "findings" and k != "new_unclassified"} |
                     {"new_unclassified_count": len(result.get("new_unclassified", []))}, indent=2))
    return 1 if result.get("new_unclassified") else 0


if __name__ == "__main__":
    raise SystemExit(main())
