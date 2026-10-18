"""Build an isolated old-package control from an existing complete host build.

Only package.cpp and ec_file_adapter.cpp see package.hpp in the current host
object graph. Rebuild both with the exact header/implementation from the named
local base commit, so std::vector<Package::OpenEntry> never has mixed layouts.
All other objects and link flags are reused, not regenerated. No checkout moves.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from e2e5_host_game_build import ROOT, command, native_git

BASE = "0f91c1ab48328f187e10f58682d318b2487176bd"
BUILD = ROOT / "port/switch/build/e2e5-host-game/optimized"


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def build(output: Path) -> dict:
    if sys.platform != "linux":
        raise ValueError("This control links a Linux host ELF; run it through the existing WSL toolchain")
    output = output.resolve()
    allowed = (ROOT / "port/switch/build/e2e5-host-game").resolve()
    if not output.is_relative_to(allowed) or output == BUILD.resolve():
        raise ValueError("control output must be separate from the candidate build")
    report_file = BUILD / "report.json"
    current = json.loads(report_file.read_text())
    candidate = Path(current["binary"])
    if current["profile"] != "optimized" or digest(candidate) != current["binary_sha256"]:
        raise ValueError("candidate report/binary mismatch or wrong profile")
    objects = [Path(s) for s in (BUILD / "link-objects.rsp").read_text().splitlines()]
    if len(objects) != current["link_objects"] or len(set(objects)) != len(objects):
        raise ValueError("object response list does not match the complete host report")
    for obj in objects:
        if not obj.is_file() or not obj.with_suffix(".d").is_file() or obj.stat().st_mtime_ns > candidate.stat().st_mtime_ns:
            raise ValueError(f"stale or missing candidate object/dependencies: {obj}")
    package_header = str(ROOT / "port/switch/platform/package.hpp")
    dependent = {obj.stem: obj for obj in objects
                 if package_header in obj.with_suffix(".d").read_text()}
    if set(dependent) != {"package", "ec_file_adapter"}:
        raise ValueError(f"package layout dependency set needs ODR review: {sorted(dependent)}")
    if output.exists():
        raise ValueError("control output already exists; retain it and choose a fresh --output")
    source = output / "source"
    source.mkdir(parents=True)
    git = native_git()
    if command([git, "rev-parse", BASE]) != BASE:
        raise ValueError("reference commit is not available")
    source_hashes = {}
    for name in ("package.hpp", "package.cpp"):
        data = subprocess.check_output([git, "show", f"{BASE}:port/switch/platform/{name}"], cwd=ROOT)
        (source / name).write_bytes(data)
        source_hashes[name] = hashlib.sha256(data).hexdigest()
    adapter = ROOT / current["source_map"]["port/switch/platform/ec_file_adapter.cpp"]
    shutil.copyfile(adapter, source / adapter.name)
    source_hashes[adapter.name] = digest(adapter)
    flags = ["-I" + str(source), *current["compile_flags"]]
    replacement = {}
    for name, original in dependent.items():
        obj = output / (name + ".o")
        command(["g++", *flags, "-MMD", "-MF", str(obj.with_suffix(".d")),
                 "-c", str(source / (name + ".cpp")), "-o", str(obj)])
        replacement[original] = obj
    selected = [replacement.get(obj, obj) for obj in objects]
    library = BUILD / "okgf/libokgf.so"
    if digest(library) != current["okgf_library_sha256"]:
        raise ValueError("candidate OKGF library changed")
    binary = output / "original-game-host-legacy-package"
    with tempfile.TemporaryDirectory(prefix="e2e5-old-package-link-") as temporary:
        staging = Path(temporary)
        staged = []
        for i, obj in enumerate(selected):
            dest = staging / f"{i:03d}.o"
            shutil.copyfile(obj, dest)
            staged.append(dest)
        response = staging / "objects.rsp"
        response.write_text("\n".join(map(str, staged)) + "\n")
        linked = staging / binary.name
        command(["g++", "@" + str(response), "-Wl,--gc-sections", str(library),
                 "-Wl,-rpath," + str(library.parent), "-lz", "-lpng", "-ljpeg",
                 "-lvorbisfile", "-lvorbis", "-logg", "-lSDL2", "-lm", "-pthread",
                 "-o", str(linked)])
        shutil.copyfile(linked, binary)
        binary.chmod(0o755)
    result = {"level": "REAL HOST CONTROL WITH EXACT ORIGINAL PACKAGE IMPLEMENTATION",
              "reference_git": BASE, "reference_files_sha256": source_hashes,
              "candidate_binary_sha256": current["binary_sha256"],
              "candidate_report_sha256": digest(report_file),
              "recompiled_package_layout_dependents": sorted(dependent),
              "reused_objects": len(objects) - len(replacement),
              "reused_object_sha256": {str(p.relative_to(ROOT)): digest(p)
                                        for p in objects if p not in replacement},
              "compile_flags": flags, "okgf_library_sha256": digest(library),
              "binary": str(binary), "binary_sha256": digest(binary),
              "limitations": ["Controls only package header/implementation; other stabilization code and seed diagnostics are shared.",
                              "Not an original Windows or Switch performance baseline."]}
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=BUILD.parent / "legacy-package-control")
    args = parser.parse_args()
    try:
        result = build(args.output)
        print(json.dumps({k: v for k, v in result.items() if k != "reused_object_sha256"}, indent=2))
        return 0
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
