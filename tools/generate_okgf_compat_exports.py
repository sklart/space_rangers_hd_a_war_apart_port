"""Generate the checked, statically linked OKGF import table from inventory."""

import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / "win32-import-manifest.json").read_text(encoding="utf-8"))
symbols = sorted({entry["symbol"] for entry in manifest["imports"]
                  if entry["dll"].lower() == "okgf.dll"})
header = (ROOT / "upstream/okgf/include/okgf.h").read_text(encoding="utf-8")
for symbol in symbols:
    if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", symbol):
        raise SystemExit(f"not a C identifier: {symbol}")
    if not re.search(r"\b" + re.escape(symbol) + r"\s*(?:\(|\)|;)", header):
        raise SystemExit(f"OKGF export missing from header: {symbol}")

lines = [
    '#include "win32_compat_okgf.hpp"',
    '#include "okgf.h"',
    'namespace srhd_awa::platform::win32_compat {',
    'ImportAddress ResolveOkgfImport(std::string_view symbol) {',
]
for symbol in symbols:
    lines.append(f'  if (symbol == "{symbol}") return reinterpret_cast<ImportAddress>(&{symbol});')
lines += ['  return nullptr;', '}', '}  // namespace srhd_awa::platform::win32_compat', '']
output = ROOT / "port/switch/platform/win32_compat_okgf.cpp"
rendered = "\n".join(lines)
if not output.exists() or output.read_text(encoding="utf-8") != rendered:
    output.write_text(rendered, encoding="utf-8")
print(f"OKGF static exports: {len(symbols)} checked")
