"""Measure a captured PCM WAV; capture origin is reported, never inferred.

The caller must supply a WAV produced by an audio test or runtime tap.  This
tool does not claim that the original GR_Music/GR_Sound event was executed.
"""

from __future__ import annotations

import argparse
from array import array
import hashlib
import json
import math
import mmap
from pathlib import Path
import subprocess
import sys
import tempfile
import wave


def analyze_pcm(data: bytes, channels: int, rate: int, origin: str,
                level: str = "OFFLINE PCM ANALYSIS") -> dict:
    samples = array("h")
    samples.frombytes(data)
    if sys.byteorder != "little":
        samples.byteswap()
    count, extra = divmod(len(samples), channels)
    if channels not in (1, 2) or rate <= 0 or extra:
        raise ValueError("invalid interleaved PCM")
    if not samples:
        raise ValueError("empty PCM data")
    squares = sum(int(x) * int(x) for x in samples)
    silent_frames = sum(all(samples[index * channels + channel] == 0 for channel in range(channels))
                        for index in range(count))
    longest = 0
    run = 0
    for index in range(count):
        zero = all(samples[index * channels + channel] == 0 for channel in range(channels))
        run = run + 1 if zero else 0
        longest = max(longest, run)
    return {"schema": 1, "level": level, "origin": origin,
            "original_game_event_verified": False, "sample_rate": rate, "channels": channels,
            "frames": count, "duration_ms": round(count * 1000 / rate, 3),
            "peak": max(abs(int(x)) for x in samples),
            "rms": round(math.sqrt(squares / len(samples)), 3),
            "dc_offset": round(sum(samples) / len(samples), 3),
            "clipped_samples": sum(x in (-32768, 32767) for x in samples),
            "silent_frames": silent_frames,
            "longest_silent_gap_ms": round(longest * 1000 / rate, 3)}


def analyze(path: Path, origin: str) -> dict:
    with wave.open(str(path), "rb") as source:
        channels, sample_width, rate, count = (source.getnchannels(), source.getsampwidth(),
                                                source.getframerate(), source.getnframes())
        if channels not in (1, 2) or sample_width != 2 or rate <= 0:
            raise ValueError("only mono/stereo signed 16-bit PCM is supported")
        data = source.readframes(count)
    if len(data) != count * channels * sample_width:
        raise ValueError("truncated PCM data")
    return analyze_pcm(data, channels, rate, origin)


def analyze_package_resource(game_root: Path, resource: str, seconds: int) -> dict:
    """Independent FFmpeg oracle for private Ogg bytes; no asset is retained."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tests"))
    from probe_ui_images_release import entries, payload
    path = game_root / "DATA/music.pkg"
    with path.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as package:
        item = next((item for item in entries(package) if item[0].casefold() == resource.casefold()), None)
        if item is None:
            raise ValueError(f"resource absent: {resource}")
        source = payload(package, item)
    if not source.startswith(b"OggS"):
        raise ValueError("resource does not contain Ogg data")
    with tempfile.TemporaryDirectory(prefix="e2e5-music-oracle-") as directory:
        source_path = Path(directory) / "music.ogg"
        source_path.write_bytes(source)
        process = subprocess.run(["ffmpeg", "-nostdin", "-v", "error", "-i", str(source_path),
                                  "-t", str(seconds), "-f", "s16le", "-acodec", "pcm_s16le",
                                  "-ac", "2", "-ar", "48000", "pipe:1"],
                                 capture_output=True)
    if process.returncode:
        raise ValueError("FFmpeg independent decode failed: " + process.stderr.decode(errors="replace")[-500:])
    result = analyze_pcm(process.stdout, 2, 48000, resource,
                         "INDEPENDENT INSTALLED MUSIC ORACLE")
    result["source_sha256"] = hashlib.sha256(source).hexdigest().upper()
    result["source_bytes"] = len(source)
    result["source_decoder"] = "FFmpeg, not original GR_Music or Switch SDL"
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("wav", type=Path, nargs="?")
    parser.add_argument("--origin", help="exact source of a supplied PCM WAV")
    parser.add_argument("--game-root", type=Path)
    parser.add_argument("--resource", help="resource path inside DATA/music.pkg")
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.game_root and args.resource and args.wav is None and 0 < args.seconds <= 120:
        report = analyze_package_resource(args.game_root, args.resource, args.seconds)
    elif args.wav and args.origin and args.game_root is None and args.resource is None:
        report = analyze(args.wav, args.origin)
    else:
        parser.error("supply WAV plus --origin, or --game-root plus --resource")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
