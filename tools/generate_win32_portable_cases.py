"""Keep the host resolver's exhaustive portable mapping test in sync with inventory."""

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--check", action="store_true")
args = parser.parse_args()
items = set()
for name in ("win32-import-manifest.json", "win32-effective-import-manifest.json"):
    manifest = json.loads((ROOT / name).read_text(encoding="utf-8"))
    for entry in manifest["imports"]:
        if entry["classification"] == "PORTABLE":
            dll = entry["dll"]
            if not dll.endswith(".dll"):
                dll += ".dll"
            items.add((dll, entry["symbol"]))
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
