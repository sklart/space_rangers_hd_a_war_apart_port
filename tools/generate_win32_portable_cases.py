"""Keep the host resolver's exhaustive portable mapping test in sync with inventory."""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
items = set()
optional_items = set()
for name in ("win32-import-manifest.json", "win32-effective-import-manifest.json"):
    manifest = json.loads((ROOT / name).read_text(encoding="utf-8"))
    for entry in manifest["imports"]:
        if entry["classification"] == "PORTABLE":
            dll = entry["dll"]
            if not dll.endswith(".dll"):
                dll += ".dll"
            items.add((dll, entry["symbol"]))
        elif entry["classification"] == "OPTIONAL_DISABLED":
            dll = entry["dll"]
            if not dll.endswith(".dll"):
                dll += ".dll"
            optional_items.add((dll, entry["symbol"]))
lines = ['// Generated from both Win32 surface manifests. Do not edit by hand.']
for dll, symbol in sorted(items):
    lines.append(f'{{"{dll}", "{symbol}"}},')
rendered = "\n".join(lines) + "\n"
path = ROOT / "tests/win32_portable_cases.inc"
if args.check:
    if not path.exists() or path.read_text(encoding="utf-8") != rendered:
        raise SystemExit("Win32 portable resolver cases are stale")
else:
    path.write_text(rendered, encoding="utf-8")
print(f"portable resolver cases: {len(items)}")
optional_lines = ['// Generated from both Win32 surface manifests. Do not edit by hand.']
for dll, symbol in sorted(optional_items):
    # The two kernel32 probes and WinMM audio timers have explicit portable
    # failure-return thunks; other disabled imports reject resolution.
    mapped_failure = dll == "winmm.dll" or (
        dll == "kernel32.dll" and symbol in
        ("GetSystemDirectoryW", "SetProcessAffinityMask"))
    optional_lines.append(f'{{"{dll}", "{symbol}", {str(mapped_failure).lower()}}},')
optional_rendered = "\n".join(optional_lines) + "\n"
optional_path = ROOT / "tests/win32_optional_cases.inc"
if args.check:
    if not optional_path.exists() or optional_path.read_text(encoding="utf-8") != optional_rendered:
        raise SystemExit("Win32 optional resolver cases are stale")
else:
    optional_path.write_text(optional_rendered, encoding="utf-8")
print(f"optional resolver cases: {len(optional_items)}")
