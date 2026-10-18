"""Prepare pinned original MatrixLib CPU sources; never edit the submodule."""
import argparse
import hashlib
import json
from pathlib import Path

INPUTS = {
    "BaseDef.hpp": "087fec96990f3e14a9988e8f1dec9c9a22ab14f2e1cc38fa68f5b3970d00f1e6",
    "CRC32.hpp": "287fb8f9b67853c487beb3b6ce96c45aa0bcdf617b348407c8bc0caed4ce69fe",
    "CRC32.cpp": "f6bf4e86be9ae18dc836e0e5dc87536e36af09c7bf33705064e4a493fafe3c76",
}

def prepare(output: Path):
    root = Path(__file__).resolve().parents[1]
    if output.resolve().is_relative_to((root / "upstream").resolve()):
        raise ValueError("Refusing to write into pinned upstream sources")
    source = root / "upstream/matrixgame/MatrixLib/Base"
    files = {name: (source / name).read_bytes().replace(b"\r\n", b"\n") for name in INPUTS}
    # Validate every input before producing any source from a different revision.
    for name, data in files.items():
        if hashlib.sha256(data).hexdigest() != INPUTS[name]:
            raise ValueError("Pinned MatrixLib input changed: " + name)
    text = files["BaseDef.hpp"].decode("utf-8")
    for old, new in [
        ("using dword = unsigned long;", "using dword = std::uint32_t;"),
        ("typedef unsigned long dword;", "typedef std::uint32_t dword;"),
    ]:
        if text.count(old) != 1:
            raise ValueError("Unexpected dword declaration")
        text = text.replace(old, new)
    text = text.replace("#pragma once", "#pragma once\n#include <cstdint>", 1)
    # Keep wchar_t untouched: the UTF-16 engine boundary needs a separate port.
    files["BaseDef.hpp"] = text.encode("utf-8")
    output.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (output / name).write_bytes(data)
    report = {
        "scope": "Original MatrixLib CRC CPU unit only; not a full engine build",
        "upstream_pin": "218115a72fa9c5d43269f4ca84a8596a0bcb82f1",
        "patch": "Both dword aliases use uint32_t; CRC algorithm unchanged",
        "utf16_port": "OPEN; wchar_t is not changed",
        "input_hash_format": "SHA256 of LF-normalized UTF-8 source",
        "inputs_sha256": INPUTS,
        "outputs_sha256": {name: hashlib.sha256(data).hexdigest() for name, data in files.items()},
    }
    (output / "source-provenance.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(prepare(args.output), indent=2))
