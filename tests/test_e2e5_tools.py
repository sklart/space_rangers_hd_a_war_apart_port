"""Regression tests for E2E-5 evidence boundaries and cache invalidation."""

from __future__ import annotations

import importlib.util
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
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
    def test_existing_okgf_archive_is_reconfigured_with_explicit_optimization(self):
        builder = module("e2e_arm64_build")
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            library = build / "okgf-no-tls/libokgf.a"
            softfloat = build / "okgf-no-tls/vendor/softfloat/libokgf_softfloat.a"
            for path in (library, softfloat):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"preexisting unconfigured archive")
            commands = []
            def run(command):
                commands.append(command)
                return 0, ""
            with mock.patch.object(builder, "BUILD", build), \
                 mock.patch.object(builder, "cmake_path", str), \
                 mock.patch.object(builder, "run", run):
                builder.build_okgf(profile="release")
                configure = [c for c in commands if "-S" in c]
                self.assertEqual(len(configure), 1)
                self.assertIn("-DCMAKE_BUILD_TYPE=Release", configure[0])
                self.assertIn("-DCMAKE_C_FLAGS_RELEASE=-O2 -g1 -DNDEBUG", configure[0])
                self.assertTrue(any("-D_Thread_local=" in flag for flag in configure[0]))
                commands.clear()
                builder.build_okgf(profile="release")
                self.assertFalse(any("-S" in c for c in commands))
                builder.build_okgf(profile="debug")
                self.assertTrue(any("-DCMAKE_BUILD_TYPE=Debug" in c for c in commands))

    def test_regenerated_okgf_table_preserves_validated_alpha_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ("tools/generate_okgf_compat_exports.py",
                             "win32-import-manifest.json", "upstream/okgf/include/okgf.h"):
                source = ROOT / relative
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(source.read_bytes())
            output = root / "port/switch/platform/win32_compat_okgf.cpp"
            output.parent.mkdir(parents=True)
            command = [sys.executable, str(root / "tools/generate_okgf_compat_exports.py")]
            subprocess.run(command, check=True, capture_output=True)
            expected = (ROOT / "port/switch/platform/win32_compat_okgf.cpp").read_text(encoding="utf-8")
            self.assertEqual(output.read_text(encoding="utf-8"), expected)
            self.assertIn('&okgf_fast_alpha::AlphaSimpleBufDraw16', expected)
            first = output.read_bytes()
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), first)

    def test_save_inventory_collects_all_values_before_rejection(self):
        probe = module("e2e5_host_game_probe")
        report = probe.parse_save_inventory(
            "E2E5_SAVE_INV|Script.One|init|Star|vkDword|STABLE_OBJECT_ID|star|7\n"
            "E2E5_SAVE_INV|Script.Two|turn|Unknown|vkInt|UNKNOWN|unresolved|0\n"
            "E2E5_SAVE_INV_SUMMARY|2|1\n")
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(len(report["entries"]), 2)
        self.assertEqual(report["entries"][0]["object_id"], 7)

    def test_host_runner_uses_matching_original_process_evidence(self):
        runner = module("e2e5_host_runner")
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "game"
            binary.write_bytes(b"original host ELF fixture")
            report = Path(directory) / "probe.json"
            probe = {"level": "ORIGINAL GAME HOST PROCESS",
                     "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                     "first_software_frame": {"width": 1280, "height": 720},
                     "main_menu_on_open_ready": True,
                     "new_game": "SCREEN DISPATCH OBSERVED",
                     "generation_complete": True,
                     "introduction_first_frame": "PRESENTATION ATTEMPT OBSERVED",
                     "dialog_choice_callback_count": 4,
                     "gameplay_screen_ids": [4], "gameplay_60_frames": True,
                     "crashed": False, "timed_out": False}
            report.write_text(json.dumps(probe), encoding="utf-8")
            matched = runner.verified_probe_report(report, binary)
            summary = runner.summarize_original_game(matched)
            self.assertEqual(summary["integrated_original_program_main"]["status"], "PASS")
            self.assertEqual(summary["new_game_to_gameplay"]["status"], "PASS")
            self.assertEqual(summary["save_load"]["status"], "NOT RUN")
            probe["crashed"] = True
            self.assertEqual(runner.summarize_original_game(probe)["new_game_to_gameplay"]["status"], "FAIL")
            binary.write_bytes(b"different ELF")
            with self.assertRaisesRegex(ValueError, "hash"):
                runner.verified_probe_report(report, binary)

    def test_independent_format0_rgb565_decoder(self):
        validator = module("e2e5_resource_validator")
        source = bytearray(98)
        source[:2] = b"gi"
        struct.pack_into("<i4i4i4i", source, 4, 1, 0, 0, 1, 1,
                         0xf800, 0x07e0, 0x001f, 0, 0, 1, 0, 0)
        struct.pack_into("<ii", source, 64, 96, 2)
        struct.pack_into("<H", source, 96, 0xf800)
        meta, pixels = validator.decode_format0(bytes(source))
        self.assertEqual((meta["width"], meta["height"]), (1, 1))
        self.assertEqual(pixels, bytes((0, 0, 248, 255)))

    def test_gamesettings_rescale_trace_is_scoped_to_disposable_source(self):
        trace = module("e2e5_cachegi_trace")
        source = (ROOT / "upstream/cpp/src/EC_CacheGI.cpp").read_text(encoding="utf-8")
        changed = trace.apply(source)
        self.assertEqual(changed.count("[E2E5] GameSettings background "), 6)
        self.assertIn("GameSettings background scale_done", changed)
        self.assertEqual(source.count("[E2E5] GameSettings background "), 0)

    def test_resource_missing_assets_is_not_pass(self):
        validator = module("e2e5_resource_validator")
        with tempfile.TemporaryDirectory() as directory:
            result = validator.validate(Path(directory))
        self.assertEqual(result["status"], "NOT RUN — ASSETS UNAVAILABLE")

    def test_original_registered_screen_inventory_and_partial_resource_status(self):
        validator = module("e2e5_resource_validator")
        original = (ROOT / "upstream/cpp/src/Globals.cpp").read_text(encoding="utf-8")
        screens = validator.registered_screen_inventory(original)
        self.assertEqual(len(screens), 36)
        self.assertEqual(len({row["screen_id"] for row in screens}), len(screens))
        self.assertEqual(next(row["config"] for row in screens
                              if row["screen_id"] == "screenNewGame"), "GameSettings")
        self.assertEqual(next(row["source_module"] for row in screens
                              if row["screen_id"] == "screenNewGame"), "fGameSettings2")
        status = validator.resource_status({"errors": [], "unsupported_oracle_images": 0,
                                            "images_not_examined": 1,
                                            "screen_dependencies_resolved": 10,
                                            "screen_image_dependencies": 10})
        self.assertTrue(status.startswith("PARTIAL"))

    def test_named_control_audit_distinguishes_other_screen_and_runtime_name(self):
        validator = module("e2e5_resource_validator")
        source = ('GetByName(u"Local"sv); '
                  'Globals::GovernmentScreen->GetByName(u"TalkText"sv); '
                  'SetName(u"Created"sv); GetByName(u"Created"sv); '
                  'GetByName(u"Absent"sv);')
        result = validator.audit_named_controls(
            source, {"Local"}, {"GovernmentScreen": {"TalkText"}})
        self.assertEqual(result["literal_references"], 4)
        self.assertEqual(result["cross_screen"], [{"target": "GovernmentScreen", "name": "TalkText"}])
        self.assertEqual(result["runtime_created"], ["Created"])
        self.assertEqual(result["unresolved"], [{"target": "current_screen", "name": "Absent"}])

    def test_gai_sequence_audit_identifies_terminal_missing_frame(self):
        validator = module("e2e5_resource_validator")
        source = bytearray(192)
        source[:4] = b"gai\0"
        struct.pack_into("<i", source, 24, 13)
        struct.pack_into("<ii", source, 32, 64, 128)
        struct.pack_into("<i", source, 64, 1)
        struct.pack_into("<i", source, 72, 16)
        struct.pack_into("<i", source, 80, 2)
        struct.pack_into("<ii", source, 84, 12, 50)
        struct.pack_into("<ii", source, 92, 13, 50)
        self.assertEqual(validator.gai_sequence_reference_issues(bytes(source)),
                         [{"sequence": 0, "position": 1, "source_frame": 13,
                           "source_frame_count": 13, "delay_ms": 50,
                           "last_in_sequence": True}])

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

    def test_gamesettings_trace_does_not_invent_later_stages(self):
        scenario = module("e2e5_scenario_runner")
        lines = ["[E2E3] screen_transition from=main_menu to=new_game",
                 "[E2E5] GameSettings background decode_begin",
                 "[E2E5] GameSettings background decode_done",
                 "[E2E5] GameSettings background scale_begin"]
        stages = {row["stage"]: row["status"] for row in scenario.evaluate(lines)["stages"]}
        self.assertEqual(stages["background_scale_begin"], "OBSERVED")
        self.assertEqual(stages["background_scale_done"], "NOT REACHED")
        self.assertEqual(stages["background_repack_done"], "NOT REACHED")

    def test_exception_info_startup_marker_is_not_failure(self):
        scenario = module("e2e5_scenario_runner")
        result = scenario.evaluate(["[E2E] ExceptionInfo", "[E2E] ProgramMain BEGIN"])
        self.assertIsNone(result["first_failure"])

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

    def test_original_menu_diagnostics_reach_effective_source(self):
        overrides = module("e2e_source_overrides")
        source = ROOT / "upstream/cpp/src/fMainForm.cpp"
        with tempfile.TemporaryDirectory() as directory:
            generated = overrides.generated_source(
                source, Path(directory) / "fMainForm.cpp", e2e5_profile=True)
            text = generated.read_text(encoding="utf-8")
        self.assertIn("menu_new_bounds left=", text)
        self.assertIn("menu_action=New press_id=", text)
        self.assertNotIn("menu_new_bounds", source.read_text(encoding="utf-8"))

    def test_dialog_pointer_payloads_keep_full_addresses_in_experiment(self):
        overrides = module("e2e_source_overrides")
        for name, expected in (("fRuinsTalk.cpp", 18), ("fTalk.cpp", 12)):
            with self.subTest(name=name):
                source = ROOT / "upstream/cpp/src" / name
                with tempfile.TemporaryDirectory() as directory:
                    generated = overrides.generated_source(
                        source, Path(directory) / name, e2e5_profile=True)
                    text = generated.read_text(encoding="utf-8")
                self.assertEqual(text.count("e2e5_pointer_payload::Register"), expected)
                self.assertIn("e2e5_pointer_payload::Resolve", text)
                self.assertNotIn("static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(", text)
                self.assertIn("e2e5_pointer_payload::ReleaseOwner(this)", text)

    def test_hangar_mirror_keeps_full_framebuffer_address_in_experiment(self):
        overrides = module("e2e_source_overrides")
        source = ROOT / "upstream/cpp/src/fHangar.cpp"
        with tempfile.TemporaryDirectory() as directory:
            generated = overrides.generated_source(
                source, Path(directory) / "fHangar.cpp", e2e5_profile=True)
            text = generated.read_text(encoding="utf-8")
        self.assertIn("std::uint8_t* cpp_left = static_cast<std::uint8_t*>(", text)
        self.assertIn("void* cpp_arg = cpp_left + Position.Y", text)
        self.assertNotIn("std::uint32_t cpp_left = static_cast<std::uint32_t>(", text)
        self.assertIn("std::uint32_t cpp_left = static_cast<std::uint32_t>(",
                      source.read_text(encoding="utf-8"))

    def test_ordinary_player_build_keeps_matching_original_declaration(self):
        overrides = module("e2e_source_overrides")
        source = ROOT / "upstream/cpp/src/aPlayer.cpp"
        with tempfile.TemporaryDirectory() as directory:
            generated = overrides.generated_source(source, Path(directory) / "aPlayer.cpp")
            text = generated.read_text(encoding="utf-8")
        self.assertIn("std::uint32_t EncodedPlayer =", text)

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

    def test_host_dependency_cache_is_scoped_to_one_build(self):
        import os
        build = module("e2e5_host_game_build")
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, header, stamp = (root / name for name in
                                     ("source.cpp", "used.hpp", "flags.json"))
            for path in (source, header, stamp):
                path.write_text("x", encoding="utf-8")
            obj = root / "source.o"
            obj.with_suffix(".d").write_text(
                f"{obj}: {source} {header}\n", encoding="utf-8")
            obj.write_text("object", encoding="utf-8")
            self.assertTrue(build.dependencies_current(obj, source, stamp, {}))
            newer = obj.stat().st_mtime_ns + 2_000_000_000
            os.utime(header, ns=(newer, newer))
            self.assertFalse(build.dependencies_current(obj, source, stamp, {}))


if __name__ == "__main__":
    unittest.main()
