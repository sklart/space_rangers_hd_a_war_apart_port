"""Artifact corruption must fail before a publishable directory exists."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ci_stage_switch_artifacts import matching_build_id, stage

SHA = "a" * 40


def binaries():
    build_id = bytes(range(20))
    elf = bytearray(156)
    elf[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", elf, 18, 183)
    struct.pack_into("<Q", elf, 32, 64)
    struct.pack_into("<HH", elf, 54, 56, 1)
    struct.pack_into("<IIQQQQQQ", elf, 64, 4, 0, 120, 0, 0, 36, 36, 4)
    struct.pack_into("<III", elf, 120, 4, 20, 3)
    elf[132:136] = b"GNU\0"
    elf[136:156] = build_id
    nro = bytearray(128)
    nro[16:20] = b"NRO0"
    struct.pack_into("<I", nro, 24, len(nro))
    nro[64:96] = build_id.ljust(32, b"\0")
    return bytes(nro), bytes(elf)


class StagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.out = self.root / "dist/switch"
        self.report_path = self.root / "report.json"
        self.report = {"build_git": SHA, "build_dirty": False, "arm64_link": "PASS",
                       "preflight": "PASS", "undefined_symbols": 0, "excluded_required": 0,
                       "required_stubs": [], "first_failure": None,
                       "experimental_e2e4_combined": True, "hardware_save_writes": False,
                       "win32_surface": {"startup_unimplemented": 0},
                       "required_game_symbols": {"Rangers::ProgramMain()": True,
                                                 "Globals::RunMainScreenStateLoop()": True},
                       "toolchain": {"compiler": "test", "devkitA64": "r30-1", "libnx": "4.12.0-1",
                                     "flags": ["-O2", "-DE2E5_DISABLE_DISK_SAVE=1"]},
                       "pinned_submodules": {name: SHA for name in ("cpp", "okgf", "matrixgame")}}
        nro, elf = binaries()
        self.report["build_id"] = matching_build_id(nro, elf)
        for key, name, data in (("nro", "game.nro", nro), ("matching_elf", "game.elf", elf)):
            (self.root / name).write_bytes(data)
            self.report[key] = {"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}

    def run_stage(self, report=None):
        self.report_path.write_text(json.dumps(self.report if report is None else report), encoding="utf-8")
        return stage(self.report_path, self.out, SHA, self.root)

    def test_exact_pair_and_metadata_only(self):
        (self.root / "private.pkg").write_bytes(b"PRIVATE")
        result = self.run_stage()
        self.assertEqual(result["hardware_status"], "HARDWARE PENDING")
        self.assertEqual({p.name for p in self.out.iterdir()},
                         {"game.nro", "game.elf", "SHA256SUMS.txt", "build-report.json", "artifact-provenance.json"})
        for line in (self.out / "SHA256SUMS.txt").read_text().splitlines():
            digest, name = line.split("  ")
            self.assertEqual(hashlib.sha256((self.out / name).read_bytes()).hexdigest(), digest)
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.run_stage()

    def test_each_required_gate_and_missing_gate_rejected(self):
        bad_values = {"build_git": "b" * 40, "build_dirty": True, "arm64_link": "FAIL",
                      "preflight": "FAIL", "undefined_symbols": 1, "excluded_required": 1,
                      "required_stubs": ["stub"], "first_failure": {},
                      "experimental_e2e4_combined": False, "hardware_save_writes": True,
                      "toolchain": {}, "pinned_submodules": {}, "build_id": "bad",
                      "win32_surface": {"startup_unimplemented": 1}, "required_game_symbols": {}}
        for key, value in bad_values.items():
            for remove in (False, True):
                with self.subTest(key=key, missing=remove):
                    bad = copy.deepcopy(self.report)
                    if remove:
                        del bad[key]
                    else:
                        bad[key] = value
                    with self.assertRaises((ValueError, KeyError)):
                        self.run_stage(bad)
                    self.assertFalse(self.out.exists())

    def test_partial_or_unknown_provenance_rejected(self):
        bad_metadata = [("required_game_symbols", {"Rangers::ProgramMain()": True}),
                        ("toolchain", {**self.report["toolchain"], "flags": ["-O2"]}),
                        ("win32_surface", {"startup_unimplemented": False}),
                        ("pinned_submodules", {"cpp": SHA}),
                        ("pinned_submodules", {name: "bad" for name in ("cpp", "okgf", "matrixgame")})]
        for field in ("compiler", "devkitA64", "libnx", "flags"):
            for value in (None, "UNKNOWN", ""):
                metadata = copy.deepcopy(self.report["toolchain"])
                metadata[field] = value
                bad_metadata.append(("toolchain", metadata))
        for key, value in bad_metadata:
            with self.subTest(key=key, value=value):
                bad = copy.deepcopy(self.report)
                bad[key] = value
                with self.assertRaises(ValueError):
                    self.run_stage(bad)
                self.assertFalse(self.out.exists())

    def test_corrupt_nro_elf_and_size_rejected(self):
        for key in ("nro", "matching_elf"):
            for field, value in (("sha256", "0" * 64), ("bytes", 0)):
                with self.subTest(key=key, field=field):
                    bad = copy.deepcopy(self.report)
                    bad[key][field] = value
                    with self.assertRaisesRegex(ValueError, "hash/size"):
                        self.run_stage(bad)
                    self.assertFalse(self.out.exists())

    def test_mismatched_build_id_even_with_valid_hash(self):
        path = self.root / "game.nro"
        data = bytearray(path.read_bytes())
        data[64] ^= 1
        path.write_bytes(data)
        self.report["nro"]["sha256"] = hashlib.sha256(data).hexdigest()
        with self.assertRaisesRegex(ValueError, "Build ID mismatch"):
            self.run_stage()
        self.assertFalse(self.out.exists())

    def test_switch_section_note_without_program_note(self):
        nro, original = binaries()
        elf = bytearray(original + bytes(64))
        struct.pack_into("<I", elf, 64, 1)  # PT_LOAD instead of PT_NOTE
        struct.pack_into("<Q", elf, 40, 156)
        struct.pack_into("<HH", elf, 58, 64, 1)
        struct.pack_into("<I", elf, 156 + 4, 7)
        struct.pack_into("<QQ", elf, 156 + 24, 120, 36)
        self.assertEqual(matching_build_id(nro, elf), self.report["build_id"])

    def test_truncated_or_wrong_architecture_binary(self):
        nro, elf = binaries()
        for bad in (elf[:12], elf[:130], elf.replace(b"\x7fELF", b"NOPE", 1)):
            with self.assertRaises((ValueError, struct.error)):
                matching_build_id(nro, bad)
        bad = bytearray(elf)
        struct.pack_into("<H", bad, 18, 62)
        with self.assertRaisesRegex(ValueError, "AArch64"):
            matching_build_id(nro, bad)

    def test_path_escape_and_absolute_path_rejected(self):
        for path in ("../game.nro", str(self.root / "game.nro")):
            bad = copy.deepcopy(self.report)
            bad["nro"]["path"] = path
            with self.assertRaises(ValueError):
                self.run_stage(bad)
            self.assertFalse(self.out.exists())


if __name__ == "__main__":
    unittest.main()
