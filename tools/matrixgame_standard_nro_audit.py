"""Read-only verification of the standard diagnostic target and saved CPU gates.

This does not run Init, GPU commands, Windows HAL, or a Switch. The output is
created exclusively, so an earlier audit cannot be silently overwritten.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from matrixgame_diagnostic_package import verify_nro

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def check_hashes(entries, base=ROOT):
    for name, digest in entries.items():
        assert sha(base / name) == digest, name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    assert build.is_relative_to(ROOT / "build"), build
    metadata = read(build / "native-diagnostic-build.json")
    check_hashes(metadata["own_source_sha256"])
    generated = read(build / "source/native-engine-source.json")["files_sha256"]
    assert generated == metadata["generated_original_sha256"]
    check_hashes(generated, build / "source")
    previous = read(ROOT / "build/matrixgame/e2e554-draw-upload/sdk/native-diagnostic-build.json")
    assert generated == previous["generated_original_sha256"]
    # Only build plumbing changed since the last complete SDK and addressed
    # CPU gates. A gameplay/provider change needs its own validation first.
    changes = [name for name, digest in previous["own_source_sha256"].items()
               if sha(ROOT / name) != digest]
    assert changes == ["port/matrixgame/native-engine/CMakeLists.txt"], changes
    added = sorted(set(metadata["own_source_sha256"]) - set(previous["own_source_sha256"]))
    assert added == ["tools/matrixgame_standard_nro_audit.py"], added

    profiles = {}
    for stage in ("e2e527-output", "e2e529-stored-state"):
        profiles[stage] = {}
        for result in sorted((ROOT / "build/matrixgame" / stage).glob("*/result.json")):
            saved = read(result)
            assert saved["status"] == "PASS", result
            assert saved["commands"] and all(c["exit"] == 0 for c in saved["commands"]), result
            drift = [name for name, digest in saved["source_sha256"].items()
                     if sha(ROOT / name) != digest]
            profiles[stage][result.parent.name] = {
                "saved_status": saved["status"], "result_sha256": sha(result),
                "source_changes_since_historical_run": drift,
                "runtime_rerun": False,
            }
        assert {"host", "arm64", "windows", "sdk"} <= profiles[stage].keys()

    baseline = read(ROOT / "reports/matrixgame-e2e521-font-evidence.json")
    protected = {name: value["sha256"] for name, value in baseline["protected"].items()
                 if value.get("status") != "UNAVAILABLE"}
    check_hashes(protected)
    check_hashes({name: value["sha256"] for name, value in baseline["extra_frozen_artifacts_verified"].items()})
    primary = baseline["primary_NRO_verified_against_previous_fingerprint"]
    assert sha(Path(primary["path"])) == primary["sha256"]
    assert sha(ROOT / "Switch-E2E5-TEST.nro") == baseline["root_cumulative_NRO_verified_against_documented_hash"]
    hardware = read(ROOT / "build/hardware-20261023/input-manifest.json")
    check_hashes({name: value["sha256"] for name, value in hardware.items()}, ROOT / "logs/2026.10.23")
    preserved = read(ROOT / "reports/matrixgame-e2e554-draw-upload-evidence.json")["preserved_packages"]
    for directory, entries in preserved.items():
        check_hashes(entries, ROOT / "dist" / directory)

    git = r"C:\Program Files\Git\cmd\git.exe"
    upstream = ROOT / "upstream/matrixgame"
    revision = subprocess.check_output([git, "-C", str(upstream), "rev-parse", "HEAD"], text=True).strip()
    assert revision == "218115a72fa9c5d43269f4ca84a8596a0bcb82f1"
    assert not subprocess.check_output([git, "-C", str(upstream), "status", "--porcelain"], text=True)
    assert subprocess.check_output([git, "rev-parse", "HEAD"], cwd=ROOT, text=True).strip() == metadata["parent"]

    sdk_root = Path("C:/devkitPro")
    package = build / "diagnostic-nro"
    nro = package / "matrixgame-native-diagnostic.nro"
    validation = verify_nro(nro.read_bytes(), (sdk_root / "libnx/default_icon.jpg").read_bytes(),
                            (package / "control.nacp").read_bytes())
    elf = build / "matrixgame-native-diagnostic.elf"
    symbols = subprocess.check_output([str(sdk_root / "devkitA64/bin/aarch64-none-elf-nm.exe"), "-C", str(elf)], text=True)
    assert not [line for line in symbols.splitlines() if " U " in line]
    for symbol in ("CGame::Init(", "CDeviceState::StoreState(", "NativeStoredStateBackend::GetMaterial(",
                   "output_paths::NativeScope::NativeScope("):
        assert symbol in symbols, symbol
    result = {
        "status": "STANDARD SDK NRO BUILD/FORMAT/SOURCE AUDIT PASS; FULL NATIVE INIT PARTIAL",
        "metadata": metadata, "nro_path": str(nro.relative_to(ROOT)), "nro_sha256": sha(nro),
        "nro_bytes": nro.stat().st_size, "elf_sha256": sha(elf), "nro_validation": validation,
        "strong_undefined": [], "unchanged_generated_original_files": len(generated),
        "source_changes_from_E554": changes, "added_build_audit_tools": added,
        "historical_profiles_reused_without_large_rerun": profiles,
        "protected_verified": protected, "hardware_inputs_verified": len(hardware),
        "primary_NRO": primary, "previous_diagnostic_packages_preserved": preserved,
        "upstream_revision": revision, "upstream_clean": True,
        "native_Init_executed_by_this_audit": False, "Switch_hardware": "NOT RUN / HARDWARE_PENDING",
        "commit": False, "push": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({key: result[key] for key in ("status", "nro_path", "nro_sha256", "nro_bytes")}, indent=2))


if __name__ == "__main__":
    main()
