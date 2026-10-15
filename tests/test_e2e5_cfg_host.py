"""Exercise the original ProgramMain twice with an isolated user CFG."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import time
from uuid import uuid4


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--game-root", type=Path, required=True)
    parser.add_argument("--library-dir", type=Path, required=True)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    game = args.game_root.resolve(strict=True)
    library = args.library_dir.resolve(strict=True)
    stage = binary.parent.parent / "runs" / ("cfg-existing-" + uuid4().hex)
    config = stage / "user/config"
    config.mkdir(parents=True)
    source = next(path for path in game.iterdir() if path.name.casefold() == "cfg.txt")
    target = config / "cfg.txt"  # Deliberately lower-case on a case-sensitive host.
    shutil.copyfile(source, target)
    marker = b"\nP0UserSetting=preserve-this-value\n"
    with target.open("ab") as output:
        output.write(marker)
    log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log"
    env = dict(os.environ, E2E5_GAME_ROOT=str(game), SDL_VIDEODRIVER="dummy",
               SDL_AUDIODRIVER="dummy", LD_LIBRARY_PATH=str(library))
    for launch in (1, 2):
        process = subprocess.Popen([str(binary)], cwd=stage, env=env,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline and process.poll() is None:
                text = log.read_text(errors="replace") if log.is_file() else ""
                if text.count("[E2E] BOOT READY") >= launch:
                    break
                time.sleep(0.2)
            else:
                raise AssertionError(f"launch {launch} did not reach BOOT READY")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if marker.strip() not in target.read_bytes():
            raise AssertionError(f"launch {launch} overwrote the user CFG")
    text = log.read_text(errors="replace")
    assert text.count("target_type=regular") >= 2
    assert text.count("[E2E] BOOT READY") == 2
    assert "[E2E] FAIL" not in text and "BOOT COMPLETE" not in text

    # The release layout remains real while only its root template is absent.
    # Symlinks keep this test isolated and avoid another copy of the game data.
    missing_stage = binary.parent.parent / "runs" / ("cfg-missing-" + uuid4().hex)
    isolated_game = missing_stage / "game"
    isolated_game.mkdir(parents=True)
    for item in game.iterdir():
        if item.name.casefold() != "cfg.txt":
            (isolated_game / item.name).symlink_to(item, target_is_directory=item.is_dir())
    missing_env = dict(env, E2E5_GAME_ROOT=str(isolated_game))
    missing_log = missing_stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log"
    result = subprocess.run([str(binary)], cwd=missing_stage, env=missing_env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                            timeout=45, check=False)
    missing_text = missing_log.read_text(errors="replace")
    assert result.returncode == 1  # ProgramMain returns, host entry reports BOOT FAILED.
    assert "FAIL stage=runtime/settings cfg=template-missing-or-invalid" in missing_text
    assert "BOOT FAILED first=FAIL stage=runtime/settings" in missing_text
    assert "BOOT READY" not in missing_text and "BOOT COMPLETE" not in missing_text
    assert not (missing_stage / "user/config/CFG.TXT").exists()
    print(f"CFG HOST PASS existing_launches=2 missing_template=BOOT_FAILED stage={stage}")


if __name__ == "__main__":
    main()
