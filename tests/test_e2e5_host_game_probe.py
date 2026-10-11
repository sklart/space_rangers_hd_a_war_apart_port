"""Regression for an observed frame falling out of the growing diagnostic tail."""

from __future__ import annotations

import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import e2e5_host_game_probe


@unittest.skipUnless(os.name == "posix", "host process fixture uses a POSIX script")
class HostGameProbeTests(unittest.TestCase):
    def test_frame_belongs_only_to_its_dispatched_screen(self) -> None:
        log = ("[E2E] screen dispatch id=33 postload=0\n"
               "[E2E] screen dispatch id=3 postload=0\n"
               "[E2E] screen Run first frame attempted\n"
               "[E2E] screen dispatch id=5 postload=0\n")
        self.assertEqual(e2e5_host_game_probe.screen_frames(log), {3})

    def test_frame_count_belongs_only_to_its_dispatched_screen(self) -> None:
        log = ("[E2E] screen dispatch id=3 postload=0\n"
               "[E2E] screen Run frames=60\n"
               "[E2E] screen dispatch id=20 postload=0\n"
               "[E2E] screen Run frames=2\n")
        self.assertEqual(e2e5_host_game_probe.screen_frame_milestones(log),
                         {3: {60}, 20: {2}})

    def test_timeout_keeps_frame_and_reports_menu_from_full_log(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            binary = base / "fixture"
            binary.write_text(
                "#!/bin/sh\n"
                "mkdir -p 'sdmc:/switch/space-rangers-hd-a-war-apart/logs'\n"
                "log='sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'\n"
                "printf '[E2E] HOST first software frame width=1280 height=720 rgb565_hash=0123456789abcdef\\n' > \"$log\"\n"
                "printf '[E2E] screen dispatch id=1 postload=0\\n[E2E] screen Run OnOpen begin\\n[E2E] screen Run OnOpen ready\\n' >> \"$log\"\n"
                "head -c 8000 /dev/zero | tr '\\000' x >> \"$log\"\n"
                "sleep 5\n", encoding="utf-8")
            binary.chmod(0o755)
            game = base / "game"
            game.mkdir()
            library = base / "lib"
            library.mkdir()
            with patch.object(e2e5_host_game_probe, "RUNS", base / "runs"):
                result = e2e5_host_game_probe.run(
                    binary, game, library, timeout=1, crash_trace=None,
                    continue_after_frame=True)
            self.assertTrue(result["timed_out"])
            self.assertEqual(result["status"], "FIRST_SOFTWARE_FRAME_OBSERVED")
            self.assertEqual(result["first_software_frame"]["rgb565_hash"],
                             "0123456789abcdef")
            self.assertTrue(result["main_menu_on_open_ready"])

    def test_crash_after_frame_is_not_reported_as_success(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            binary = base / "fixture"
            binary.write_text(
                "#!/bin/sh\n"
                "mkdir -p 'sdmc:/switch/space-rangers-hd-a-war-apart/logs'\n"
                "printf '[E2E] HOST first software frame width=1280 height=720 rgb565_hash=0123456789abcdef\\n' > 'sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'\n"
                "printf 'E2E5 HOST SIGNAL 11 address=0x8a\\n' >&2\n"
                "exit 139\n", encoding="utf-8")
            binary.chmod(0o755)
            game = base / "game"
            game.mkdir()
            library = base / "lib"
            library.mkdir()
            with patch.object(e2e5_host_game_probe, "RUNS", base / "runs"):
                result = e2e5_host_game_probe.run(
                    binary, game, library, timeout=2, crash_trace=None,
                    continue_after_frame=True)
            self.assertTrue(result["crashed"])
            self.assertEqual(result["status"], "CRASH_AFTER_FIRST_FRAME")
            self.assertIsNotNone(result["first_software_frame"])

    def test_nonzero_exit_after_frame_is_not_reported_as_success(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            binary = base / "fixture"
            binary.write_text(
                "#!/bin/sh\n"
                "mkdir -p 'sdmc:/switch/space-rangers-hd-a-war-apart/logs'\n"
                "printf '[E2E] HOST first software frame width=1280 height=720 rgb565_hash=0123456789abcdef\\n' > 'sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'\n"
                "exit 134\n", encoding="utf-8")
            binary.chmod(0o755)
            game = base / "game"
            game.mkdir()
            library = base / "lib"
            library.mkdir()
            with patch.object(e2e5_host_game_probe, "RUNS", base / "runs"):
                result = e2e5_host_game_probe.run(
                    binary, game, library, timeout=2, crash_trace=None,
                    continue_after_frame=True)
            self.assertEqual(result["status"], "PROCESS_ERROR_AFTER_FIRST_FRAME")


if __name__ == "__main__":
    unittest.main()
