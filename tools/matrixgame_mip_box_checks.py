"""Compare a frozen revision and current mip preparation on the CPU."""

import io, zipfile, argparse, hashlib, json, re, subprocess, statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GIT = "C:/Program Files/Git/cmd/git.exe"
FROZEN = "c32f35b8bc4fc8977095a22c34dbf2813f9c14a0"


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def linux(p):
    return str(p).replace("\\", "/").replace("D:/", "/mnt/d/").replace("C:/", "/mnt/c/")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kind", choices=("host", "arm64", "windows", "benchmark"))
    ap.add_argument("--reference", default=FROZEN)
    ap.add_argument("--run-name", default="e610-mip-box-r37-checks")
    ap.add_argument(
        "--benchmark-source", default="tests/benchmark_matrixgame_mip_copy.cpp"
    )
    args = ap.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.run_name):
        ap.error("--run-name must be a single directory name")
    reference = subprocess.check_output(
        [
            GIT,
            "rev-parse",
            "--verify",
            "--end-of-options",
            args.reference + "^{commit}",
        ],
        cwd=ROOT,
        text=True,
    ).strip()
    kind = args.kind
    parent = ROOT / "build/matrixgame" / args.run_name / kind
    parent.mkdir(parents=True, exist_ok=True)
    i = 1
    while (parent / str(i)).exists():
        i += 1
    out = parent / str(i)
    out.mkdir()
    j = {
        "status": "INCOMPLETE",
        "commands": [],
        "scope": "real frozen/candidate CPU validator; no Switch/GPU/FPS",
    }
    tree = out / "frozen-tree"
    tree.mkdir()
    blob = subprocess.check_output(
        [GIT, "archive", "--format=zip", reference, "port/matrixgame"], cwd=ROOT
    )
    with zipfile.ZipFile(io.BytesIO(blob)) as archive:
        assert all(
            not Path(n).is_absolute()
            and ".." not in Path(n).parts
            and n.startswith("port/")
            for n in archive.namelist()
        )
        archive.extractall(tree)
    test = (
        args.benchmark_source
        if kind == "benchmark"
        else "tests/test_matrixgame_mip_copy.cpp"
    )
    current = "port/matrixgame/native-core/texture_mips.cpp"
    bc = ROOT / "build/matrixgame/e610-mip-copy-r36/1/core/source/BC.cpp"

    def run(cmd, label):
        cmd = list(map(str, cmd))
        p = subprocess.run(
            cmd,
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        log = p.stdout + p.stderr
        (out / (label + ".log")).write_text(log, encoding="utf-8")
        j["commands"].append({"label": label, "command": cmd, "exit": p.returncode})
        print(
            kind,
            label,
            p.returncode,
            log[-2500:] if p.returncode or label.startswith("runtime") else "",
            flush=True,
        )
        assert p.returncode == 0, label
        return log

    try:
        programs = {}
        measurements = []
        for variant in ("frozen", "candidate"):
            base = tree if variant == "frozen" else ROOT
            includes = [
                base / "port/matrixgame" / n
                for n in ("native-engine", "native-core", "deko")
            ] + [
                bc.parent,
                base / "port/matrixgame/third_party/directxmath/Inc",
                base / "port/matrixgame/third_party/sal",
            ]
            files = [ROOT / test, base / current, bc]
            exe = out / variant
            if kind == "windows":
                obj = out / (variant + "-objects")
                obj.mkdir()
                vs = r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
                cmd = (
                    f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /MT /EHsc /std:c++20 /O2 /UNDEBUG /fp:strict '
                    + " ".join('/I"' + str(p) + '"' for p in includes)
                    + " "
                    + " ".join('"' + str(p) + '"' for p in files)
                    + f' /Fo"{obj.as_posix()}/" /Fe"{exe}.exe"'
                )
                script = out / (variant + ".cmd")
                script.write_text(cmd + "\n")
                run(["cmd.exe", "/d", "/c", script], "build-" + variant)
                programs[variant] = [str(exe) + ".exe"]
            else:
                flags = [
                    "-std=c++20",
                    "-O2",
                    "-g",
                    "-UNDEBUG",
                    "-ffp-contract=off",
                    "-fno-pie",
                    "-no-pie",
                    "-fno-omit-frame-pointer",
                    "-frounding-math",
                    "-fno-tree-sink",
                ]
                if kind == "host":
                    flags += ["-fsanitize=address,undefined"]
                if kind == "arm64":
                    version = out / "local-typeinfo.map"
                    version.write_text("{ local: _ZNKSt9type_infoeqERKS_; };\n")
                    flags += [
                        "-fsanitize=undefined",
                        "-Wl,--version-script=" + linux(version),
                    ]
                run(
                    [
                        "wsl",
                        "--exec",
                        "aarch64-linux-gnu-g++-15" if kind == "arm64" else "g++",
                        *flags,
                        *["-I" + linux(p) for p in includes],
                        *map(linux, files),
                        "-o",
                        linux(exe),
                    ],
                    "build-" + variant,
                )
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
            for i, variant in enumerate(
                ("frozen", "candidate", "candidate", "frozen", "frozen", "candidate")
            ):
                log = run(programs[variant], "runtime-" + variant + "-" + str(i))
                m = {k: int(v) for k, v in re.findall(r"\b(\w+_us|checksum)=(\d+)", log)}
                measurements.append({"variant": variant, **m})
            assert len({m["checksum"] for m in measurements}) == 1
            timing_keys = sorted(k for k in measurements[0] if k.endswith("_us"))
            assert timing_keys and all(
                set(m) == {"variant", "checksum", *timing_keys} for m in measurements
            )
            j["measurements"] = measurements
            j["median_us"] = {
                v: {
                    k: statistics.median(
                        m[k] for m in measurements if m["variant"] == v
                    )
                    for k in timing_keys
                }
                for v in programs
            }
        else:
            outputs = {v: run(cmd, "runtime-" + v) for v, cmd in programs.items()}
            assert outputs["candidate"] == outputs["frozen"], "frozen behavior differs"
            assert "MIP_COPY" in outputs["candidate"]
            j["semantic_stdout"] = outputs["candidate"]
            j["alignment_stdout"] = run(
                programs["candidate"] + ["--unaligned-bc"],
                "runtime-candidate-unaligned-bc",
            )
            assert "BC_ALIGNMENT cases=864" in j["alignment_stdout"]
        j["status"] = (
            "PASS FROZEN/CANDIDATE CPU O2 COMPARISON; GUARDS RETAINED; HARDWARE_PENDING"
        )
    finally:
        paths = [
            test,
            current,
            "port/matrixgame/native-core/texture_asset.hpp",
            "port/matrixgame/native-core/bc_native_prelude.hpp",
            "tools/matrixgame_mip_box_checks.py",
        ]
        j["source_sha256"] = {p: sha(ROOT / p) for p in paths}
        j["frozen_commit"] = reference
        j["frozen_cpp_sha256"] = sha(tree / current)
        j["shared_BC"] = {
            "path": str(bc.relative_to(ROOT)),
            "sha256": sha(bc),
            "scope": "unchanged real prepared BC.cpp used by both variants",
        }
        (out / "result.json").write_text(
            json.dumps(j, indent=2) + "\n", encoding="utf-8"
        )


if __name__ == "__main__":
    main()
