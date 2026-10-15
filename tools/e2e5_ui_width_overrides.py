"""Disposable native-width UI payload repair for the combined experiment.

Only the transient TObjectGI UserValue/UserIndex cells change width.  Numeric
payloads retain their values.  Original upstream files remain byte-for-byte.
"""

from __future__ import annotations

import re

PTR = re.compile(r"static_cast<std::int32_t>\(reinterpret_cast<std::uintptr_t>\(([^()]+)\)\)")
READ = re.compile(r"static_cast<std::uintptr_t>\(static_cast<std::uint32_t>\(([^()]*->User(?:Value|Index))\)\)")
FIELD = re.compile(r"\bUser(?:Value|Index)\b")


def apply(relative: str, source: str) -> str:
    if relative == "types/GI_MessageLoop.hpp":
        for name in ("UserValue", "UserIndex"):
            old = f"std::int32_t {name};"
            if source.count(old) != 1:
                raise RuntimeError(f"UI width source boundary changed: {name}")
            source = source.replace(old, f"std::intptr_t {name};", 1)
        return source
    if not relative.endswith(".cpp") or not FIELD.search(source):
        return source
    lines = source.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if not FIELD.search(line):
            continue
        # Pointer writes and comparisons that intentionally compare pointer
        # identities.  Do not change other integer payload casts.
        if "reinterpret_cast<std::uintptr_t>" in line and (
            re.search(r"->User(?:Value|Index)\s*=", line) or
            re.search(r"->User(?:Value|Index)\s*==", line)):
            line = PTR.sub(r"reinterpret_cast<std::intptr_t>(\1)", line)
        # The reader is rewritten only when its expression casts to a pointer.
        if "reinterpret_cast<" in line:
            line = READ.sub(r"static_cast<std::uintptr_t>(\1)", line)
        lines[index] = line
    return "".join(lines)
