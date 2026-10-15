"""Disposable native-width UI payload repair for the combined experiment.

Only the transient TObjectGI UserValue/UserIndex/UserData/UserState cells change width. Numeric
payloads retain their values.  Original upstream files remain byte-for-byte.
"""

from __future__ import annotations

import re

READ = re.compile(r"static_cast<std::uintptr_t>\(static_cast<std::uint32_t>\(([^()]*->User(?:Value|Index|Data|State))\)\)")
FIELD = re.compile(r"\bUser(?:Value|Index|Data|State)\b")

def widen_pointer_casts(line: str) -> str:
    prefix = 'static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>('
    while (start := line.find(prefix)) >= 0:
        begin = start + len(prefix)
        end, depth = begin, 1
        while depth:
            if end >= len(line): raise RuntimeError('unbalanced UI pointer cast')
            if line[end] == '(': depth += 1
            elif line[end] == ')': depth -= 1
            end += 1
        if line[end:end+1] != ')': raise RuntimeError('UI pointer cast boundary changed')
        line = line[:start] + 'reinterpret_cast<std::intptr_t>(' + line[begin:end-1] + ')' + line[end+1:]
    return line


def apply(relative: str, source: str) -> str:
    if relative == "types/GI_MessageLoop.hpp":
        for name in ("UserValue", "UserIndex", "UserData", "UserState"):
            old = f"std::int32_t {name};"
            if source.count(old) != (2 if name == 'UserData' else 1):
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
            re.search(r"->User(?:Value|Index|Data|State)\s*=", line) or
            re.search(r"->User(?:Value|Index|Data|State)\s*==", line)):
            line = widen_pointer_casts(line)
        # The reader is rewritten only when its expression casts to a pointer.
        if "reinterpret_cast<" in line:
            line = READ.sub(r"static_cast<std::uintptr_t>(\1)", line)
        lines[index] = line
    return "".join(lines)
