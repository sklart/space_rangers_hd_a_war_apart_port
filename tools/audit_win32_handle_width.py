"""Reject pointer-to-32-bit conversions in synthetic Win32 handle code."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1] / "port/switch/platform"
PATTERNS = (
    re.compile(r"reinterpret_cast\s*<\s*(?:std::)?u?int32_t\s*>"),
    re.compile(r"(?:static_cast\s*<\s*(?:std::)?u?int32_t\s*>|"
               r"\(\s*(?:std::)?u?int32_t\s*\))\s*\(\s*"
               r"reinterpret_cast\s*<\s*(?:std::)?u?intptr_t"),
)

errors = []
for path in sorted(ROOT.glob("win32_compat*.cpp")) + [ROOT / "win32_handles.cpp"]:
    content = path.read_text(encoding="utf-8")
    for pattern in PATTERNS:
        for match in pattern.finditer(content):
            errors.append(f"{path.name}:{content.count(chr(10), 0, match.start()) + 1}: "
                          "pointer narrowed to 32-bit handle")
if errors:
    raise SystemExit("\n".join(errors))
print("synthetic Win32 handle width: PASS")
