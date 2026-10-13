"""Compare captured original-game SDL PCM with the logged package track.

Requires FFmpeg, NumPy and SciPy on the host. Extracted game audio is kept only
in a temporary directory and is never added to the repository.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mmap
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def compare(report_path: Path, game_root: Path) -> dict:
    import numpy as np
    from scipy import signal

    sys.path.insert(0, str(ROOT / "tests"))
    from probe_ui_images_release import entries, payload

    report = json.loads(report_path.read_text(encoding="utf-8"))
    evidence = report["audio"]["capture"]
    if not report["audio"]["pcm_output_verified"] or evidence["driver"] != "disk":
        raise ValueError("report does not verify original-game SDL disk PCM")
    rate = evidence["sample_rate"]
    if evidence["channels"] != 2 or evidence["sdl_format"] != 32784 or rate <= 0:
        raise ValueError("expected little-endian stereo S16 output")
    captured_path = report_path.parent / "audio-output.raw"
    captured_bytes = captured_path.read_bytes()
    if len(captured_bytes) > 16 * 1024 * 1024 or len(captured_bytes) % 4:
        raise ValueError("invalid bounded PCM capture")
    if hashlib.sha256(captured_bytes).hexdigest().upper() != evidence["sha256"]:
        raise ValueError("capture hash does not match the probe report")
    log = (report_path.parent / "user/config/########.log").read_text(
        encoding="utf-8", errors="replace")
    choices = re.findall(r"(?im)^music[\\/][^\r\n]+\.dat$", log)
    if len(choices) != 1:
        raise ValueError(f"expected one logged music track, found {len(choices)}")
    resource = choices[0].replace("\\", "/")
    with (game_root / "DATA/music.pkg").open("rb") as stream, mmap.mmap(
        stream.fileno(), 0, access=mmap.ACCESS_READ) as package:
        item = next((row for row in entries(package)
                     if row[0].casefold() == resource.casefold()), None)
        if item is None:
            raise ValueError(f"logged track missing from package: {resource}")
        ogg = payload(package, item)

    ffmpeg = shutil.which("ffmpeg")
    if ffmpeg is None:
        raise ValueError("FFmpeg is required for the independent decoder")
    with tempfile.TemporaryDirectory(prefix="e2e5-audio-oracle-") as temporary:
        source = Path(temporary) / "source.ogg"
        reference = Path(temporary) / "reference.raw"
        source.write_bytes(ogg)
        subprocess.run([ffmpeg, "-nostdin", "-v", "error", "-i", str(source),
                        "-f", "s16le", "-acodec", "pcm_s16le", "-ac", "2",
                        "-ar", str(rate), str(reference)], check=True)
        captured = np.frombuffer(captured_bytes, dtype="<i2").reshape(-1, 2)
        decoded = np.fromfile(reference, dtype="<i2").reshape(-1, 2)
        active = np.flatnonzero(np.any(captured != 0, axis=1))
        if not len(active):
            raise ValueError("captured output contains no non-silent frames")
        first_active = int(active[0])
        output_mono = captured[first_active:].mean(axis=1).astype(np.float64)
        source_mono = decoded.mean(axis=1).astype(np.float64)
        # Search the entire logged track, since the original player may seek.
        factor = 50
        output_low = signal.resample_poly(output_mono - output_mono.mean(), 1, factor)
        source_low = signal.resample_poly(source_mono - source_mono.mean(), 1, factor)
        correlation = signal.correlate(output_low, source_low, mode="full", method="fft")
        lag = (int(np.argmax(np.abs(correlation))) - (len(source_low) - 1)) * factor
        best = (0.0, lag)
        for correction in range(-factor * 2, factor * 2 + 1):
            candidate = lag + correction
            out_start = max(candidate, 0)
            src_start = max(-candidate, 0)
            length = min(rate * 2, len(output_mono) - out_start,
                         len(source_mono) - src_start)
            if length < rate:
                continue
            left = output_mono[out_start:out_start + length]
            right = source_mono[src_start:src_start + length]
            denominator = np.sqrt(np.dot(left, left) * np.dot(right, right))
            score = float(np.dot(left, right) / denominator) if denominator else 0.0
            if abs(score) > abs(best[0]):
                best = (score, candidate)
        lag = best[1]
        out_start = max(lag, 0)
        src_start = max(-lag, 0)
        length = min(rate * 30, len(output_mono) - out_start,
                     len(source_mono) - src_start)
        if length < rate * 5:
            raise ValueError("less than five seconds of overlapping audio")
        left = output_mono[out_start:out_start + length]
        right = source_mono[src_start:src_start + length]
        coefficient = float(np.corrcoef(left, right)[0, 1])
        gain = float(np.dot(left, right) / np.dot(right, right))
        return {
            "schema": 1, "level": "ORIGINAL GAME HOST VS INDEPENDENT DECODER",
            "capture_sha256": evidence["sha256"],
            "source_sha256": hashlib.sha256(ogg).hexdigest().upper(),
            "logged_resource": resource, "sample_rate": rate,
            "first_audible_seconds": round(first_active / rate, 3),
            "alignment_seconds": round(lag / rate, 6),
            "compared_seconds": round(length / rate, 3),
            "correlation": round(coefficient, 6),
            "gain_ratio": round(gain, 6),
            "matches_reference": coefficient >= 0.95,
            "hardware_audio_verified": False,
        }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-report", type=Path, required=True)
    parser.add_argument("--game-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = compare(args.capture_report, args.game_root)
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
