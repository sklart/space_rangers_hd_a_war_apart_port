"""Frozen-r39 versus r40 CPU endpoint benchmark, plus diagnostic lifetime checks."""

import argparse
import hashlib
import io
import json
import os
import statistics
import subprocess
import zipfile
from pathlib import Path

from matrixgame_windows_host_env import compiler, environment

ROOT = Path(__file__).resolve().parents[1]
GIT = "C:/Program Files/Git/cmd/git.exe"
R39 = "c1116e01469e443cb5b9fd94ebfee8e3f8845b4d"
SDK = Path("C:/devkitPro/libnx/include")
SOURCES = {
    "uniform": ["tests/test_matrixgame_uniform_slice.cpp"],
    "session": ["tests/test_matrixgame_diagnostic_session.cpp"],
    "journal": [
        "tests/test_matrixgame_diagnostic_journal.cpp",
        "port/matrixgame/native-engine/native_diagnostic_journal.cpp",
    ],
}


def linux(path):
    value = path.resolve().as_posix()
    return f"/mnt/{value[0].lower()}/{value[3:]}"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("host", "arm64", "windows", "benchmark"))
    mode = parser.parse_args().platform
    parent = ROOT / "build/matrixgame/e610-r40-checks" / mode
    index = 1
    while (parent / str(index)).exists():
        index += 1
    output = parent / str(index)
    output.mkdir(parents=True)
    frozen = output / "frozen-r39"
    frozen.mkdir()
    data = subprocess.check_output(
        [GIT, "archive", "--format=zip", R39, "port/matrixgame"], cwd=ROOT
    )
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        assert all(
            name.startswith("port/") and ".." not in Path(name).parts
            for name in archive.namelist()
        )
        archive.extractall(frozen)

    env = environment(output) if mode == "windows" else dict(os.environ)
    result = {
        "status": "INCOMPLETE",
        "reference": R39,
        "commands": [],
        "platform": mode,
    }
    paths = {
        Path(__file__).resolve(),
        *(ROOT / path for sources in SOURCES.values() for path in sources),
        *(
            ROOT / "port/matrixgame/native-engine" / name
            for name in (
                "native_draw_upload.hpp",
                "native_upload_reuse.hpp",
                "native_draw_batch.hpp",
                "completed_block_pool.hpp",
                "native_diagnostic_session.hpp",
                "native_diagnostic_journal.hpp",
            )
        ),
        ROOT / "port/matrixgame/deko/material_uniform_packet.hpp",
    }
    result["source_sha256"] = {
        str(path.relative_to(ROOT)): sha(path) for path in sorted(paths)
    }

    def run(command, label):
        proc = subprocess.run(
            list(map(str, command)),
            cwd=output,
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        (output / f"{label}.log").write_text(
            proc.stdout + proc.stderr, encoding="utf-8"
        )
        result["commands"].append(
            {
                "command": list(map(str, command)),
                "exit": proc.returncode,
                "label": label,
            }
        )
        print(
            label,
            proc.returncode,
            proc.stdout.strip() if not proc.returncode else proc.stderr[-3000:],
            flush=True,
        )
        proc.check_returncode()
        return proc.stdout

    def build(name, variant="current"):
        root = frozen if variant == "r39" else ROOT
        include = [
            root / "port/matrixgame" / part
            for part in ("native-engine", "native-core", "deko")
        ] + [SDK]
        sources = [ROOT / path for path in SOURCES[name]]
        label = f"{name}-{variant}"
        target = output / label
        define = variant == "r39"
        if mode == "windows":
            target = target.with_suffix(".exe")
            run(
                [
                    compiler(env),
                    "/nologo",
                    "/std:c++20",
                    "/EHsc",
                    "/MT",
                    "/O2",
                    "/UNDEBUG",
                    "/utf-8",
                    *(["/DFROZEN_R39"] if define else []),
                    *["/I" + str(path) for path in include],
                    *sources,
                    "/Fe" + str(target),
                ],
                "build-" + label,
            )
            return [target]
        cxx = "aarch64-linux-gnu-g++-15" if mode == "arm64" else "g++"
        flags = [
            "-std=c++20",
            "-O2",
            "-g",
            "-UNDEBUG",
            "-fno-pie",
            "-no-pie",
            "-pthread",
        ]
        if mode == "host":
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        if mode == "arm64":
            flags += ["-fsanitize=undefined"]
        run(
            [
                "wsl",
                "--exec",
                cxx,
                *flags,
                *(["-DFROZEN_R39"] if define else []),
                *["-I" + linux(path) for path in include],
                *map(linux, sources),
                "-o",
                linux(target),
            ],
            "build-" + label,
        )
        runtime = [
            "wsl",
            "--exec",
            "env",
            "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1",
            "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1",
        ]
        if mode == "arm64":
            runtime += ["qemu-aarch64", "-L", "/usr/aarch64-linux-gnu"]
        return runtime + [linux(target)]

    try:
        programs = {
            "r39": build("uniform", "r39") + ["separate"],
            "r40": build("uniform") + ["combined"],
        }
        readings = []
        order = (
            ("r39", "r40", "r40", "r39", "r39", "r40")
            if mode == "benchmark"
            else ("r39", "r40")
        )
        for i, variant in enumerate(order):
            line = run(programs[variant], f"{variant}-{i}")
            values = {
                name: int(value)
                for name, value in (pair.split("=") for pair in line.split())
            }
            readings.append({"variant": variant, **values})
        result["readings"] = readings
        for field in ("draws", "submits", "requested", "hash"):
            assert len({row[field] for row in readings}) == 1, field
        assert max(row["creates"] for row in readings if row["variant"] == "r40") < min(
            row["creates"] for row in readings if row["variant"] == "r39"
        )
        result["medians"] = {
            variant: {
                key: statistics.median(
                    r[key] for r in readings if r["variant"] == variant
                )
                for key in readings[0]
                if key != "variant"
            }
            for variant in programs
        }
        if mode != "benchmark":
            result["session"] = run(build("session"), "session")
            assert "phase_deadlines=PASS" in result["session"]
            journal_dir = output / "filesystem"
            path = str(journal_dir) if mode == "windows" else linux(journal_dir)
            result["journal"] = run(build("journal") + [path], "journal")
            assert "DIAGNOSTIC_JOURNAL PASS" in result["journal"]
            confirmed = []
            for candidate in journal_dir.rglob("run.jsonl"):
                try:
                    rows = [
                        json.loads(line)
                        for line in candidate.read_text(encoding="utf-8").splitlines()
                    ]
                except (UnicodeError, json.JSONDecodeError):
                    continue  # The journal fixture deliberately injects partial writes.
                if any(
                    row["event"] == "cancelled" and row["operation"] == "ControlledDraw"
                    for row in rows
                ):
                    assert (
                        rows[-1]["event"] == "journal_close" and rows[-1]["result"] == 0
                    )
                    assert [row["seq"] for row in rows] == list(range(1, len(rows) + 1))
                    confirmed.append(sha(candidate))
            assert len(confirmed) == 1
            result["cancelled_journal_sha256"] = confirmed[0]
        result["status"] = "PASS"
    finally:
        (output / "result.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8"
        )


if __name__ == "__main__":
    main()
