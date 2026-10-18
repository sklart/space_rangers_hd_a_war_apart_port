"""Fail-closed tests for the generation comparator, not synthetic gameplay proof."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / "tools"
spec = importlib.util.spec_from_file_location("generation_compare", TOOLS / "e2e5_compare_generation.py")
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class GenerationComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.patch = patch.object(compare, "RUNS", self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)
        self.addCleanup(self.temp.cleanup)

    def fixture(self, name, scale=1):
        stage = self.root / name
        config = stage / "user/config"
        config.mkdir(parents=True)
        for filename in ("CFG.TXT", "Lang.txt", "newgame.txt"):
            (config / filename).write_text(filename)
        rows = []
        for turn, final in [(i, False) for i in range(0, 301, 25)] + [(300, True)]:
            rows.append(dict(warmup_turn=turn, final=final, seed=42,
                             galaxy_rng=900, rtl_rng=77, stars=7, planets=19, ships=21,
                             star_key_hash="123", planet_key_hash="456", ship_key_hash="789",
                             elapsed_us=(turn + len(rows) + 1) * 1000 * scale))
        (stage / "generation-checkpoints.jsonl").write_text("\n".join(map(json.dumps, rows)) + "\n")
        (stage / "generation-world.bin").write_bytes(b"full world")
        sdmc = "sdmc\uf03a" if sys.platform == "win32" else "sdmc:"
        log = stage / sdmc / "switch/space-rangers-hd-a-war-apart/logs/e2e.log"
        log.parent.mkdir(parents=True)
        log.write_text("HOST generation fixture seed=42 rtl_state=17 chaotic=0\n"
                       "HOST generation snapshot complete bytes=10 galaxy_rng_before=900 after=901 "
                       "rtl_rng_before=77 after=77 save_count_before=0 after=1\n")
        report = self.root / (name + ".json")
        report.write_text(json.dumps(dict(returncode=0, crashed=False, timed_out=False,
                                         stopped_by_probe=False, generation_complete=True,
                                         stage=str(stage), binary_sha256="one exact binary",
                                         resource_configuration={"main_dat_sha256": "one input"})))
        return report, stage, log

    def test_elapsed_time_does_not_mask_world_or_rng_comparison(self):
        reports = [self.fixture(f"r{i}", i + 1)[0] for i in range(3)]
        proof = compare.compare(reports)
        self.assertEqual(proof["status"], "PASS")
        self.assertEqual(len(proof["runs"]), 3)
        self.assertEqual(len(set(proof["generation_seconds"])), 3)

    def test_offscreen_entity_rng_difference_is_rejected(self):
        fixtures = [self.fixture(f"r{i}") for i in range(3)]
        path = fixtures[1][1] / "generation-checkpoints.jsonl"
        rows = [json.loads(x) for x in path.read_text().splitlines()]
        rows[4]["planet_key_hash"] = "changed at checkpoint 100"
        path.write_text("\n".join(map(json.dumps, rows)))
        proof = compare.compare([f[0] for f in fixtures])
        self.assertEqual(proof["status"], "FAIL")
        self.assertIn("checkpoints", proof["differences"][0]["fields"])

    def test_full_world_change_is_rejected_even_with_equal_key_hashes(self):
        fixtures = [self.fixture(f"r{i}") for i in range(3)]
        (fixtures[2][1] / "generation-world.bin").write_bytes(b"full worle")
        proof = compare.compare([f[0] for f in fixtures])
        self.assertEqual(proof["status"], "FAIL")
        self.assertIn("world_payload_sha256", proof["differences"][1]["fields"])

    def test_rtl_rng_at_a_world_checkpoint_cannot_be_ignored(self):
        fixtures = [self.fixture(f"r{i}") for i in range(3)]
        path = fixtures[1][1] / "generation-checkpoints.jsonl"
        rows = [json.loads(x) for x in path.read_text().splitlines()]
        rows[0]["rtl_rng"] += 1
        path.write_text("\n".join(map(json.dumps, rows)))
        proof = compare.compare([f[0] for f in fixtures])
        self.assertEqual(proof["status"], "FAIL")
        self.assertIn("checkpoints", proof["differences"][0]["fields"])

    def test_different_native_settings_are_rejected(self):
        fixtures = [self.fixture(f"r{i}") for i in range(3)]
        (fixtures[1][1] / "user/config/newgame.txt").write_text("Name=another player")
        proof = compare.compare([f[0] for f in fixtures])
        self.assertEqual(proof["status"], "FAIL")
        self.assertIn("config_sha256", proof["differences"][0]["fields"])

    def test_duplicate_run_cannot_count_as_three_measurements(self):
        report, _, _ = self.fixture("one")
        copies = [self.root / f"copy{i}.json" for i in range(3)]
        for path in copies:
            path.write_bytes(report.read_bytes())
        with self.assertRaisesRegex(ValueError, "distinct native run"):
            compare.compare(copies)

    def test_missing_terminal_marker_is_rejected(self):
        report, _, log = self.fixture("one")
        log.write_text("generation stage=8 status=complete\n")
        with self.assertRaisesRegex(ValueError, "snapshot marker"):
            compare.inspect(report)

    def test_truncated_payload_is_rejected(self):
        report, stage, _ = self.fixture("one")
        (stage / "generation-world.bin").write_bytes(b"partial")
        with self.assertRaisesRegex(ValueError, "payload size"):
            compare.inspect(report)


if __name__ == "__main__":
    unittest.main()
