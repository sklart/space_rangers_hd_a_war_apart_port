"""Regression tests for E2E-5 evidence boundaries and cache invalidation."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))


def module(name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / f"{name}.py")
    assert spec and spec.loader
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


class E2E5ToolTests(unittest.TestCase):
    def test_resource_missing_assets_is_not_pass(self):
        validator = module("e2e5_resource_validator")
        with tempfile.TemporaryDirectory() as directory:
            result = validator.validate(Path(directory))
        self.assertEqual(result["status"], "NOT RUN — ASSETS UNAVAILABLE")

    def test_first_failure_precedes_secondary_crash(self):
        analyzer = module("e2e5_crash_analyzer")
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "report.zip"
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("e2e.log", "[E2E] build_git=" + "a" * 40 +
                           "\n[E2E] FAIL stage=screen draw original exception\n")
                z.writestr("original.log", "Rescaling Bm.Test... ")
                z.writestr("crash_reports/one.log", "Type: Instruction Abort\n"
                           "ReturnAddress[00]: 0000 (Game + 0x1234)\n")
            result = analyzer.analyze(archive, None, None, None)
        self.assertIn("screen draw", result["first_game_failure"]["text"])
        self.assertEqual(result["unfinished_original_stage"]["text"], "Rescaling Bm.Test...")
        self.assertEqual(result["crash_reports"][0]["first_offset"], "0x1234")

    def test_pointer_inventory_detects_new_narrowing(self):
        audit = module("e2e5_static_audit")
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "sample.cpp"
            source.write_text("auto value = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));\n", encoding="utf-8")
            original_roots = audit.SOURCE_ROOTS
            original_root = audit.ROOT
            try:
                audit.ROOT = Path(directory)
                audit.SOURCE_ROOTS = (Path(directory),)
                result = audit.inventory()
            finally:
                audit.SOURCE_ROOTS = original_roots
                audit.ROOT = original_root
        self.assertGreater(result["finding_count"], 0)
        self.assertGreater(result["unresolved_count"], 0)

    def test_game_settings_frame_requires_new_game_transition(self):
        scenario = module("e2e5_scenario_runner")
        lines = ["[E2E] screen Run first frame attempted",
                 "[E2E3] screen_transition from=main_menu to=new_game",
                 "[E2E] screen Run OnOpen ready"]
        stages = {row["stage"]: row["status"] for row in scenario.evaluate(lines)["stages"]}
        self.assertEqual(stages["game_settings_on_open"], "OBSERVED")
        self.assertEqual(stages["game_settings_first_frame"], "NOT REACHED")

    def test_pcm_metrics_find_clip_dc_and_silence(self):
        audio = module("e2e5_audio_capture")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sample.wav"
            with wave.open(str(path), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(1000)
                wav.writeframes(bytes((0, 0, 0, 0, 255, 127, 0, 0)))
            result = audio.analyze(path, "synthetic unittest")
        self.assertEqual(result["clipped_samples"], 1)
        self.assertEqual(result["silent_frames"], 3)
        self.assertEqual(result["longest_silent_gap_ms"], 2)
        self.assertFalse(result["original_game_event_verified"])

    def test_disposable_ui_payload_repair_preserves_numeric_uses(self):
        repair = module("e2e5_ui_width_overrides")
        source = (ROOT / "upstream/cpp/src/fCfgSettings.cpp").read_text(encoding="utf-8")
        changed = repair.apply("fCfgSettings.cpp", source)
        self.assertIn("Slider->UserIndex = reinterpret_cast<std::intptr_t>(ValueLabel);", changed)
        self.assertIn("static_cast<std::uintptr_t>(Sender->UserIndex)", changed)
        self.assertNotIn("static_cast<std::uint32_t>(Sender->UserIndex)", changed)
        header = (ROOT / "upstream/cpp/src/types/GI_MessageLoop.hpp").read_text(encoding="utf-8")
        widened = repair.apply("types/GI_MessageLoop.hpp", header)
        self.assertIn("std::intptr_t UserValue;", widened)
        self.assertIn("std::intptr_t UserIndex;", widened)
        self.assertIn("std::int32_t UserData;", widened)

    def test_all_known_ui_pointer_payload_sites_are_rewritten(self):
        repair = module("e2e5_ui_width_overrides")
        source_root = ROOT / "upstream/cpp/src"
        changed_files = 0
        original_writes = 0
        for path in source_root.glob("*.cpp"):
            before = path.read_text(encoding="utf-8")
            original_writes += len(re.findall(r"->User(?:Value|Index)\s*=\s*static_cast<std::int32_t>\(reinterpret_cast", before))
            after = repair.apply(path.name, before)
            if before != after:
                changed_files += 1
            for line in after.splitlines():
                if "UserValue" not in line and "UserIndex" not in line:
                    continue
                if re.search(r"->User(?:Value|Index)\s*=\s*static_cast<std::int32_t>\(reinterpret_cast", line):
                    self.fail(f"pointer write remains at {path.name}: {line.strip()}")
                if "reinterpret_cast<" in line and re.search(
                    r"static_cast<std::uintptr_t>\(static_cast<std::uint32_t>\([^()]*->User(?:Value|Index)\)\)", line):
                    self.fail(f"pointer read remains at {path.name}: {line.strip()}")
        self.assertGreaterEqual(original_writes, 25)
        self.assertGreaterEqual(changed_files, 15)

    def test_original_screen_stage_profile_is_disposable(self):
        overrides = module("e2e_source_overrides")
        source = ROOT / "upstream/cpp/src/GI_MessageLoop.cpp"
        with tempfile.TemporaryDirectory() as directory:
            generated = overrides.generated_source(source, Path(directory) / "GI_MessageLoop.cpp",
                                                   e2e5_profile=True)
            text = generated.read_text(encoding="utf-8")
        self.assertIn("e2e5_stage_profiler::Window e2e_profile", text)
        self.assertGreater(text.count("e2e_profile.Enter("), 20)
        self.assertNotIn("e2e_profile", source.read_text(encoding="utf-8"))

    def test_depfile_change_invalidates_only_dependent_object(self):
        build = module("e2e_arm64_build")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, header, other = (root / name for name in ("source.cpp", "used.hpp", "unused.hpp"))
            for path in (source, header, other):
                path.write_text("x", encoding="utf-8")
            obj = root / "source.o"
            dep = root / "source.d"
            stamp = root / "flags.txt"
            stamp.write_text("same", encoding="utf-8")
            dep.write_text(f"{obj}: {source} {header}\n", encoding="utf-8")
            obj.write_text("object", encoding="utf-8")
            self.assertTrue(build.object_current(obj, dep, [source], stamp))
            other.write_text("new", encoding="utf-8")
            self.assertTrue(build.object_current(obj, dep, [source], stamp))
            import os
            newer = obj.stat().st_mtime_ns + 2_000_000_000
            os.utime(header, ns=(newer, newer))
            self.assertFalse(build.object_current(obj, dep, [source], stamp))


if __name__ == "__main__":
    unittest.main()
