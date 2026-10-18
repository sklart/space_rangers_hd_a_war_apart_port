"""Fail-closed staging of the exact combined Switch NRO and matching ELF."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]


def span(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError("truncated binary")
    return data[offset:offset + size]


def elf_build_id(data: bytes) -> bytes:
    if span(data, 0, 6) != b"\x7fELF\x02\x01":
        raise ValueError("expected little-endian ELF64")
    if struct.unpack_from("<H", data, 18)[0] != 183:
        raise ValueError("expected AArch64 ELF")
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phsize, phcount = struct.unpack_from("<HH", data, 54)
    if phsize != 56 or not phcount:
        raise ValueError("invalid program headers")
    ids = set()
    note_ranges = set()
    for i in range(phcount):
        ph = span(data, phoff + i * phsize, phsize)
        kind, = struct.unpack_from("<I", ph)
        if kind != 4:  # PT_NOTE
            continue
        offset, = struct.unpack_from("<Q", ph, 8)
        size, = struct.unpack_from("<Q", ph, 32)
        note_ranges.add((offset, size))
    # The Switch linker retains .note.gnu.build-id as SHT_NOTE without a
    # PT_NOTE segment. Linux ELF fixtures alone cannot cover this layout.
    shoff = struct.unpack_from("<Q", data, 40)[0]
    shsize, shcount = struct.unpack_from("<HH", data, 58)
    if shcount and shsize != 64:
        raise ValueError("invalid section headers")
    for i in range(shcount):
        section = span(data, shoff + i * shsize, shsize)
        kind, = struct.unpack_from("<I", section, 4)
        if kind == 7:  # SHT_NOTE
            note_ranges.add(struct.unpack_from("<QQ", section, 24))
    for offset, size in note_ranges:
        notes = span(data, offset, size)
        at = 0
        while at < len(notes):
            namesz, descsz, note_type = struct.unpack("<III", span(notes, at, 12))
            at += 12
            name = span(notes, at, namesz)
            at += (namesz + 3) & ~3
            desc = span(notes, at, descsz)
            at += (descsz + 3) & ~3
            if note_type == 3 and name == b"GNU\0":
                if not 1 <= len(desc) <= 32:
                    raise ValueError("invalid GNU Build ID length")
                ids.add(bytes(desc))
    if len(ids) != 1:
        raise ValueError("missing or ambiguous GNU Build ID")
    return ids.pop()


def matching_build_id(nro: bytes, elf: bytes) -> str:
    if span(nro, 16, 4) != b"NRO0":
        raise ValueError("invalid NRO magic")
    size, = struct.unpack_from("<I", nro, 24)
    if size < 128 or size > len(nro):
        raise ValueError("invalid NRO size")
    build_id = elf_build_id(elf)
    if span(nro, 64, 32) != build_id.ljust(32, b"\0"):
        raise ValueError("NRO/ELF Build ID mismatch")
    return build_id.hex()


def stage(report_path: Path, out: Path, expected_git: str, root: Path = ROOT,
          save_mode: str = "disabled") -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", expected_git):
        raise ValueError("expected full Git SHA")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if save_mode not in ("disabled", "experimental"):
        raise ValueError("invalid save mode")
    required = {"build_git": expected_git, "build_dirty": False,
                "arm64_link": "PASS", "preflight": "PASS", "undefined_symbols": 0,
                "excluded_required": 0, "required_stubs": [], "first_failure": None,
                "experimental_e2e4_combined": True,
                "hardware_save_writes": save_mode == "experimental",
                "save_mode": save_mode, "save_format": "RSG64/v168",
                "build_profile": "release"}
    for key, value in required.items():
        if key not in report or type(report[key]) is not type(value) or report[key] != value:
            raise ValueError(f"report gate failed: {key}")
    startup = report.get("win32_surface", {}).get("startup_unimplemented")
    if type(startup) is not int or startup != 0:
        raise ValueError("unimplemented startup imports")
    symbols = report.get("required_game_symbols", {})
    required_symbols = {"Rangers::ProgramMain()", "Globals::RunMainScreenStateLoop()"}
    if not required_symbols.issubset(symbols) or not all(value is True for value in symbols.values()):
        raise ValueError("missing original game symbols")
    metadata = report.get("toolchain")
    pins = report.get("pinned_submodules")
    if not isinstance(metadata, dict) or not isinstance(pins, dict):
        raise ValueError("missing toolchain or submodule provenance")
    for name in ("compiler", "devkitA64", "libnx"):
        if not isinstance(metadata.get(name), str) or metadata[name] in ("", "UNKNOWN"):
            raise ValueError("missing toolchain version: " + name)
    if not isinstance(metadata.get("flags"), list) or not metadata["flags"] or not all(
            isinstance(flag, str) for flag in metadata["flags"]):
        raise ValueError("missing compiler flags")
    flags = metadata["flags"]
    disable_flags = [flag for flag in flags if flag.startswith("-DE2E5_DISABLE_DISK_SAVE")]
    if disable_flags != ([] if save_mode == "experimental" else ["-DE2E5_DISABLE_DISK_SAVE=1"]):
        raise ValueError("compiler flags disagree with save mode")
    if "-O2" not in flags or any(flag in flags for flag in ("-O0", "-O3", "-Ofast", "-ffast-math")):
        raise ValueError("unexpected release compiler profile")
    if set(pins) != {"cpp", "okgf", "matrixgame"} or not all(
            isinstance(revision, str) and re.fullmatch(r"[0-9a-f]{40}", revision)
            for revision in pins.values()):
        raise ValueError("missing pinned submodule revisions")
    root = root.resolve()
    files = {}
    for key, extension in (("nro", ".nro"), ("matching_elf", ".elf")):
        entry = report[key]
        relative = Path(entry["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("artifact path must be checkout-relative")
        path = (root / relative).resolve(strict=True)
        if not path.is_relative_to(root) or path.suffix != extension:
            raise ValueError("artifact path escapes checkout or has wrong type")
        if save_mode == "experimental" and "save-test-v168" not in path.name:
            raise ValueError("experimental artifact must be named save-test-v168")
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != entry["bytes"] or digest != entry["sha256"].lower():
            raise ValueError(f"artifact hash/size mismatch: {key}")
        files[key] = (path, data, digest)
    build_id = matching_build_id(files["nro"][1], files["matching_elf"][1])
    if report.get("build_id") != build_id:
        raise ValueError("report Build ID mismatch")
    out = out.resolve()
    if out.exists():
        raise ValueError("staging directory already exists")
    if not out.is_relative_to(root) or out == root:
        raise ValueError("staging directory must be inside checkout")
    provenance = {"schema": 1, "build_git": expected_git, "build_dirty": False,
                  "build_id": build_id, "hardware_status": "HARDWARE PENDING",
                  "hardware_save_writes": save_mode == "experimental",
                  "save_mode": save_mode, "save_format": "RSG64/v168",
                  "release_ready": False, "toolchain": metadata,
                  "pinned_submodules": pins,
                  "ci": {"run_id": os.environ.get("GITHUB_RUN_ID"),
                         "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
                         "repository": os.environ.get("GITHUB_REPOSITORY"),
                         "status": "REQUIRED JOBS PASSED" if os.environ.get("GITHUB_ACTIONS") == "true" else "LOCAL STAGING; CI UNVERIFIED"},
                  "files": {}}
    # Validate everything before creating any publishable output. Copy only the
    # two explicit binary paths; no glob can pull private resources into staging.
    out.mkdir(parents=True)
    try:
        sums = []
        for key, (path, data, digest) in files.items():
            (out / path.name).write_bytes(data)
            sums.append(f"{digest}  {path.name}\n")
            provenance["files"][key] = {"name": path.name, "bytes": len(data), "sha256": digest}
        (out / "SHA256SUMS.txt").write_text("".join(sums), encoding="utf-8")
        # The build report contains relative artifact paths and sanitized tool
        # versions/flags, never the local compiler executable or private assets.
        (out / "build-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        (out / "artifact-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    except BaseException:
        shutil.rmtree(out)
        raise
    return provenance


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--expected-git", required=True)
    parser.add_argument("--save-mode", choices=("disabled", "experimental"), default="disabled")
    args = parser.parse_args()
    try:
        result = stage(args.report, args.out, args.expected_git, save_mode=args.save_mode)
    except (ValueError, OSError, KeyError, TypeError, struct.error) as error:
        parser.exit(1, f"Switch artifact staging FAILED: {error}\n")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
