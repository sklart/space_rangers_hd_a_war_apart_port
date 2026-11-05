"""Compare real system-memory texture publication against the r38 source."""

import argparse
import hashlib
import json
import re
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GIT = "C:/Program Files/Git/cmd/git.exe"
REFERENCE = "c45e2604e6c1e24329b72a2e520fe30143a540a7"
SOURCE = Path("port/matrixgame/native-engine/native_texture.cpp")
FIXTURE = Path("tests/benchmark_matrixgame_texture_publish.cpp")
CHECKS = ROOT / "build/matrixgame/e610-texture-publish-r39-checks"


def linux(path):
    path = Path(path).resolve().as_posix()
    return f"/mnt/{path[0].lower()}/{path[3:]}"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("host", "arm64", "windows", "benchmark"))
    kind = parser.parse_args().kind
    directory = CHECKS / kind
    directory.mkdir(parents=True, exist_ok=True)
    index = 1
    while (directory / str(index)).exists():
        index += 1
    out = directory / str(index)
    out.mkdir()

    platform = "host" if kind == "benchmark" else kind
    prior = ROOT / "build/matrixgame/e2e521/residency" / platform / "result.json"
    commands = {
        entry["label"]: entry["command"]
        for entry in json.loads(prior.read_text(encoding="utf-8"))["commands"]
    }
    frozen = out / "frozen_native_texture.cpp"
    frozen.write_bytes(
        subprocess.check_output(
            [GIT, "show", f"{REFERENCE}:{SOURCE.as_posix()}"], cwd=ROOT
        )
    )
    result = {
        "status": "INCOMPLETE",
        "reference": REFERENCE,
        "frozen_sha256": sha(frozen),
        "command_template_sha256": sha(prior),
        "source_sha256": {
            str(path.as_posix()): sha(ROOT / path) for path in (SOURCE, FIXTURE)
        },
        "commands": [],
        "scope": "real Publish and ValidateUpload, system-memory pool 2, no GPU upload",
    }

    def run(command, label):
        command = list(map(str, command))
        proc = subprocess.run(
            command, cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace"
        )
        (out / f"{label}.log").write_text(proc.stdout + proc.stderr, encoding="utf-8")
        result["commands"].append(
            {"label": label, "command": command, "exit": proc.returncode}
        )
        print(label, proc.returncode, (proc.stdout + proc.stderr)[-2400:], flush=True)
        proc.check_returncode()
        return proc.stdout.strip()

    def windows(command, label):
        script = out / f"{label}.cmd"
        script.write_text(command + "\n", encoding="utf-8")
        run(["cmd.exe", "/d", "/c", script], label)

    try:
        programs = {}
        for variant, source in (("frozen", frozen), ("candidate", ROOT / SOURCE)):
            unit = out / f"{variant}.cpp"
            include_path = Path.as_posix if kind == "windows" else linux
            unit.write_text(
                f'#include "{include_path(source)}"\n#include "{include_path(ROOT / FIXTURE)}"\n',
                encoding="utf-8",
            )
            exe = out / variant
            if kind == "windows":
                original_dir = prior.parent
                obj = out / f"{variant}.obj"
                compile_command = commands["native_texture"].replace("\\\\", "\\")
                compile_command = compile_command.replace(str(ROOT / SOURCE), str(unit))
                compile_command = compile_command.replace(
                    str(original_dir / "native_texture.obj"), str(obj)
                )
                windows(compile_command, "build-" + variant)
                link = commands["link"].replace("\\\\", "\\")
                link = link.replace(
                    str(original_dir / "texture-residency.exe"), str(exe) + ".exe"
                )
                link = link.replace(str(original_dir / "native_texture.obj"), str(obj))
                link = link.replace(
                    f'"{original_dir / "test_matrixgame_texture_residency.obj"}" ', ""
                )
                mip_object = (
                    ROOT
                    / "build/matrixgame/e610-managed-r4-checks/windows/2/texture_mips.obj"
                )
                result["shared_mip_object_sha256"] = sha(mip_object)
                link = link.replace(" kernel32.lib", f' "{mip_object}" kernel32.lib')
                windows(link, "link-" + variant)
                programs[variant] = [str(exe) + ".exe"]
            else:
                command = [
                    arg for arg in commands["build-link"] if not arg.endswith(".cpp")
                ]
                command[command.index("-O1")] = "-O2"
                command[command.index("-o") + 1] = linux(exe)
                first_archive = next(
                    i for i, arg in enumerate(command) if arg.endswith(".a")
                )
                command.insert(first_archive, linux(unit))
                if kind == "benchmark":
                    release = (
                        ROOT
                        / "build/matrixgame/e2e522/clean-host-release/libmatrixgame-native-core.a"
                    )
                    result["shared_release_archive_sha256"] = sha(release)
                    command = [
                        linux(release)
                        if arg.endswith("/libmatrixgame-native-core.a")
                        else arg
                        for arg in command
                    ]
                    command = [
                        arg for arg in command if not arg.startswith("-fsanitize=")
                    ]
                if kind == "arm64":
                    version_map = out / "local-typeinfo.map"
                    version_map.write_text("{ local: _ZNKSt9type_infoeqERKS_; };\n")
                    command += [
                        "-fsanitize=undefined",
                        "-Wl,--version-script=" + linux(version_map),
                    ]
                run(command, "build-" + variant)
                runtime = [
                    "wsl",
                    "--exec",
                    "env",
                    "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1",
                    "UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1",
                ]
                if kind == "arm64":
                    runtime += ["qemu-aarch64", "-L", "/usr/aarch64-linux-gnu"]
                programs[variant] = runtime + [linux(exe)]

        if kind == "benchmark":
            readings = []
            metrics = [f"f{fmt}_us" for fmt in (21, 22, 23, 25, 26)]
            for index, variant in enumerate(
                ("frozen", "candidate", "candidate", "frozen", "frozen", "candidate")
            ):
                output = run(
                    programs[variant] + ["benchmark"], f"runtime-{variant}-{index}"
                )
                row = {
                    key: int(value)
                    for key, value in re.findall(
                        r"(f[0-9]+_us|checksum)=([0-9]+)", output
                    )
                }
                assert set(row) == {"checksum", *metrics}
                readings.append({"variant": variant, **row})
            assert len({row["checksum"] for row in readings}) == 1
            result["measurements"] = readings
            result["median_us"] = {
                variant: {
                    key: statistics.median(
                        row[key] for row in readings if row["variant"] == variant
                    )
                    for key in metrics
                }
                for variant in programs
            }
        else:
            outputs = {
                variant: run(program, "runtime-" + variant)
                for variant, program in programs.items()
            }
            assert outputs["frozen"] == outputs["candidate"]
            assert "PUBLISH_PIXELS cases=360" in outputs["candidate"]
            result["semantic_stdout"] = outputs["candidate"]
        result["status"] = "PASS"
    finally:
        (out / "result.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8"
        )


if __name__ == "__main__":
    main()
