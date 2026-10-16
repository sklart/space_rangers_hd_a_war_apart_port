"""Build the complete effective original game for an isolated Linux host probe.

The same disposable source generators used by the experimental Switch build
are applied here.  This is a host diagnostic executable, never a replacement
for the ARM64 build or a hardware test.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile

from e2e4_script_width_overrides import prepare_script_source_overlay
from e2e5_cachegi_trace import apply as apply_cachegi_trace
from e2e5_ui_width_overrides import apply as apply_ui_width_overrides
from e2e_runtime_overrides import prepare_runtime_overlay
from e2e_source_overrides import generated_source


ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "upstream/cpp"
SWITCH = ROOT / "port/switch"
BUILD = SWITCH / "build/e2e5-host-game/reproducible"
PATCH_NAMES = ("gr_main_portable.patch", "gr_main_zlib_portable.patch",
               "dat_zlib_guard.patch")
PATCHED_NAMES = ("GR_Main.cpp", "EC_BlockPar.cpp", "EC_Data.cpp")
PLATFORM_NAMES = (
    "e2e_calendar.cpp", "e2e_events.cpp", "e2e_file_match.cpp",
    "e2e_file_search.cpp", "e2e_threads.cpp", "e2e4_game_audio_bridge.cpp",
    "e2e4_native_vorbis.cpp", "e2e4_quicksave_transaction.cpp",
    "e2e4_sdl_audio.cpp", "ec_file_adapter.cpp", "game_path.cpp",
    "input_platform.cpp", "okgf_bridge.cpp", "package.cpp",
    "renderer_platform.cpp", "runtime_platform.cpp", "user_root.cpp",
    "win32_compat_clipboard.cpp", "win32_compat_files.cpp",
    "win32_compat_filetime.cpp", "win32_compat_gdi.cpp",
    "win32_compat_heap.cpp", "win32_compat_messages.cpp",
    "win32_compat_modules.cpp", "win32_compat_okgf.cpp",
    "win32_compat_process.cpp", "win32_compat_registry.cpp",
    "win32_compat_sync.cpp", "win32_compat_text.cpp",
    "win32_compat_window.cpp", "win32_compat_winmm.cpp",
    "win32_compat.cpp", "win32_handles.cpp", "zlib_bridge.cpp",
)


def command(args: list[str], cwd: Path = ROOT) -> str:
    result = subprocess.run(args, cwd=cwd, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {args[:3]}\n"
                           + (result.stdout + result.stderr)[-4000:])
    return result.stdout.strip()


def native_git() -> str:
    if sys.platform.startswith("linux") and Path("/mnt/c/Program Files/Git/cmd/git.exe").is_file():
        return "/mnt/c/Program Files/Git/cmd/git.exe"
    return shutil.which("git") or "git"


def patched_sources() -> dict[str, Path]:
    destination = BUILD / "patched-upstream"
    stamp = destination / "patch-inputs.json"
    inputs = [*(GAME / "src" / name for name in PATCHED_NAMES),
              *(SWITCH / "platform" / name for name in PATCH_NAMES)]
    digest = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in inputs}
    expected = json.dumps(digest, sort_keys=True, indent=2) + "\n"
    if not (stamp.is_file() and stamp.read_text(encoding="utf-8") == expected and
            all((destination / "src" / name).is_file() for name in PATCHED_NAMES)):
        (destination / "src").mkdir(parents=True, exist_ok=True)
        for name in PATCHED_NAMES:
            shutil.copyfile(GAME / "src" / name, destination / "src" / name)
        for name in PATCH_NAMES:
            patch = SWITCH / "platform" / name
            if native_git().endswith("git.exe") and sys.platform.startswith("linux"):
                patch_arg = command(["wslpath", "-w", str(patch)])
            else:
                patch_arg = str(patch)
            command([native_git(), "apply",
                     "--directory=" + destination.relative_to(ROOT).as_posix(),
                     "--ignore-space-change", patch_arg])
        if "portable user root is unavailable" not in (
                destination / "src/GR_Main.cpp").read_text(encoding="utf-8"):
            raise RuntimeError("GR_Main disposable portability patch missing")
        stamp.write_text(expected, encoding="utf-8")
    return {name: destination / "src" / name for name in PATCHED_NAMES}


def build_okgf(profile: str) -> Path:
    location = BUILD / "okgf"
    command(["cmake", "-S", str(ROOT / "upstream/okgf"), "-B", str(location),
             "-DOKGF_MATH_BACKEND=NATIVE",
             "-DCMAKE_BUILD_TYPE=" + ("Release" if profile == "optimized" else "Debug")])
    command(["cmake", "--build", str(location), "--parallel", "2"])
    library = location / "libokgf.so"
    if not library.is_file():
        raise RuntimeError("host OKGF library missing")
    return library


def source_overlay() -> Path:
    destination = BUILD / "source-overlay"
    stamp = BUILD / "source-overlay-inputs.json"
    tracked_source_changes = command(
        [native_git(), "status", "--porcelain", "--", "upstream/cpp/src"])
    scripts = (ROOT / "tools/e2e4_script_width_overrides.py",
               ROOT / "tools/e2e59_overrides.py",
               ROOT / "tools/e2e5_ui_width_overrides.py")
    fingerprint = {
        "git_head": command([native_git(), "rev-parse", "HEAD"]),
        "upstream_status": tracked_source_changes,
        "generators": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in scripts},
    }
    rendered = json.dumps(fingerprint, sort_keys=True, indent=2) + "\n"
    required = ("types/aScript.hpp", "types/EC_Expression.hpp",
                "types/GI_MessageLoop.hpp", "aScript.cpp", "aScriptFun.cpp",
                "EC_Expression.cpp")
    if (not tracked_source_changes and stamp.is_file() and
            stamp.read_text(encoding="utf-8") == rendered and
            all((destination / name).is_file() for name in required)):
        return destination
    prepare_script_source_overlay(GAME / "src", destination)
    stamp.write_text(rendered, encoding="utf-8")
    return destination


def source_map() -> tuple[list[tuple[Path, Path]], dict[str, str]]:
    overlay = source_overlay()
    prepare_runtime_overlay(GAME / "runtime", BUILD / "runtime-overlay")
    patched = patched_sources()
    cache_path = BUILD / "effective-source-cache.json"
    generators = (ROOT / "tools/e2e_source_overrides.py",
                  ROOT / "tools/e2e510_overrides.py",
                  ROOT / "tools/e2e58_file_overrides.py",
                  ROOT / "tools/e2e59_overrides.py",
                  ROOT / "tools/e2e56_cache_overrides.py",
                  ROOT / "tools/e2e56_geometry_overrides.py",
                  ROOT / "tools/e2e5_ui_width_overrides.py",
                  ROOT / "tools/e2e5_cachegi_trace.py")
    generator_hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in generators}
    try:
        previous = json.loads(cache_path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        previous = {}
    cached_entries = (previous.get("entries", {})
                      if previous.get("generator_hashes") == generator_hashes else {})
    next_entries = {}
    mapping: list[tuple[Path, Path]] = []
    manifest: dict[str, str] = {}
    originals = sorted((GAME / "src").glob("*.cpp")) + [GAME / "runtime/runtime.cpp"]
    for source in originals:
        if source.name in ("program.cpp", "EC_HsFile.cpp"):
            manifest[source.relative_to(ROOT).as_posix()] = "replaced by host entrypoint / portable EC_File adapter"
            continue
        source_input = patched.get(source.name,
            overlay / source.name if source.parent == GAME / "src" else source)
        key = source.relative_to(ROOT).as_posix()
        source_stat = source_input.stat()
        fingerprint = [str(source_input.relative_to(ROOT)), source_stat.st_size,
                       source_stat.st_mtime_ns]
        prior = cached_entries.get(key, {})
        candidate = ROOT / prior.get("effective", "") if prior.get("effective") else None
        if prior.get("fingerprint") == fingerprint and candidate and candidate.is_file():
            effective = candidate
        else:
            effective = generated_source(source_input, BUILD / "generated" / source.name,
                                         audio_on=True, e2e5_profile=True,
                                         script_width=True)
            if source.parent == GAME / "src":
                current = effective.read_text(encoding="utf-8")
                rendered = apply_ui_width_overrides(source.name, current)
                if source.name == "EC_CacheGI.cpp":
                    rendered = apply_cachegi_trace(rendered)
                if rendered != current:
                    effective = BUILD / "generated-ui" / source.name
                    effective.parent.mkdir(parents=True, exist_ok=True)
                    if not effective.is_file() or effective.read_text(encoding="utf-8") != rendered:
                        effective.write_text(rendered, encoding="utf-8")
        next_entries[key] = {"fingerprint": fingerprint,
                             "effective": effective.relative_to(ROOT).as_posix()}
        mapping.append((source, effective))
        manifest[source.relative_to(ROOT).as_posix()] = effective.relative_to(ROOT).as_posix()
    for name in PLATFORM_NAMES:
        source = SWITCH / "platform" / name
        mapping.append((source, source))
        manifest[source.relative_to(ROOT).as_posix()] = source.relative_to(ROOT).as_posix()
    main = ROOT / "tools/e2e5_host_game_main.cpp"
    mapping.append((main, main))
    manifest[main.relative_to(ROOT).as_posix()] = main.relative_to(ROOT).as_posix()
    cache_path.write_text(json.dumps({"generator_hashes": generator_hashes,
                                      "entries": next_entries}, sort_keys=True) + "\n",
                          encoding="utf-8")
    return mapping, manifest


def dependencies_current(obj: Path, effective: Path, flags_stamp: Path,
                         file_stamps: dict[Path, tuple[bool, int]]) -> bool:
    depfile = obj.with_suffix(".d")
    if not obj.is_file() or not depfile.is_file():
        return False
    rendered = depfile.read_text(encoding="utf-8", errors="replace")
    rendered = rendered.replace("\\\r\n", " ").replace("\\\n", " ")
    colon = rendered.find(": ")
    if colon < 0:
        return False
    prerequisites = [Path(name.replace("\\ ", " ")) for name in
                     re.split(r"(?<!\\)\s+", rendered[colon + 2:].strip()) if name]
    threshold = obj.stat().st_mtime_ns
    if not prerequisites:
        return False
    for path in (effective, flags_stamp, *prerequisites):
        if path not in file_stamps:
            try:
                info = path.stat()
                file_stamps[path] = (stat.S_ISREG(info.st_mode), info.st_mtime_ns)
            except OSError:
                file_stamps[path] = (False, 0)
        regular, modified = file_stamps[path]
        if not regular or modified > threshold:
            return False
    return True


def main() -> int:
    global BUILD
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--profile", choices=("functional", "debug", "optimized"),
                        default="functional")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.profile != "functional":
        BUILD = SWITCH / "build/e2e5-host-game" / args.profile
    BUILD.mkdir(parents=True, exist_ok=True)
    git = native_git()
    build_git = command([git, "rev-parse", "HEAD"])
    dirty = bool(command([git, "status", "--porcelain", "--untracked-files=all"]))
    library = build_okgf(args.profile)
    mapping, manifest = source_map()
    profile_flags = {"functional": ("-O0", "-g0"),
                     "debug": ("-O0", "-g1"),
                     "optimized": ("-O2", "-g1", "-fno-strict-aliasing")}
    flags = ["-std=gnu++20", *profile_flags[args.profile], "-DE2E_HOST_RESOLVER_TEST",
             "-DE2E3_TRACE=1",
             "-DE2E5_HOST_SDL_WINDOW", "-DSRHD_PORTABLE_RENDERER",
             "-DE2E4_AUDIO_ON=1", "-DE2E4_QUICKSAVE_EXPERIMENT=1",
             "-ffunction-sections", "-fdata-sections", "-Wno-invalid-offsetof",
             *["-I" + str(path) for path in
               (SWITCH / "platform", BUILD / "source-overlay", GAME / "src",
                BUILD / "runtime-overlay", GAME / "runtime",
                ROOT / "upstream/okgf/include")]]
    flags_stamp = BUILD / "compile-flags.json"
    rendered_flags = json.dumps({"compiler": command(["g++", "--version"]).splitlines()[0],
                                 "flags": flags}, sort_keys=True) + "\n"
    if not flags_stamp.is_file() or flags_stamp.read_text(encoding="utf-8") != rendered_flags:
        flags_stamp.write_text(rendered_flags, encoding="utf-8")
    objects: list[Path] = []
    work: list[tuple[Path, Path]] = []
    file_stamps: dict[Path, tuple[bool, int]] = {}
    for original, effective in mapping:
        category = "platform" if original.parent == SWITCH / "platform" else (
            "host" if original.parent == ROOT / "tools" else "upstream")
        obj = BUILD / "objects" / category / (original.stem + ".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        objects.append(obj)
        if not dependencies_current(obj, effective, flags_stamp, file_stamps):
            work.append((effective, obj))

    def compile_one(entry: tuple[Path, Path]) -> None:
        source, obj = entry
        command(["g++", *flags, "-MMD", "-MF", str(obj.with_suffix(".d")),
                 "-c", str(source), "-o", str(obj)])

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(compile_one, entry): entry for entry in work}
        for future in as_completed(futures):
            try:
                future.result()
            except RuntimeError as error:
                raise RuntimeError(f"compile {futures[future][0]}: {error}") from error
    binary = BUILD / "original-game-host"
    response = BUILD / "link-objects.rsp"
    rendered_response = "\n".join(str(obj) for obj in objects) + "\n"
    if not response.is_file() or response.read_text(encoding="utf-8") != rendered_response:
        response.write_text(rendered_response, encoding="utf-8")
    need_link = (not binary.is_file() or any(
        path.stat().st_mtime_ns > binary.stat().st_mtime_ns
        for path in (*objects, library, response)))
    if need_link:
        # GNU ld repeatedly seeks through C++ objects.  Link from the Linux
        # temp filesystem so WSL does not issue thousands of random NTFS reads.
        with tempfile.TemporaryDirectory(prefix="e2e5-host-link-") as temporary:
            staging = Path(temporary)
            staged = []
            for index, obj in enumerate(objects):
                target = staging / f"{index:03d}.o"
                shutil.copyfile(obj, target)
                staged.append(target)
            staged_response = staging / "objects.rsp"
            staged_response.write_text("\n".join(str(obj) for obj in staged) + "\n",
                                       encoding="utf-8")
            staged_binary = staging / "original-game-host"
            command(["g++", "@" + str(staged_response), "-Wl,--gc-sections",
                     str(library), "-Wl,-rpath," + str(library.parent),
                     "-lz", "-lpng", "-ljpeg", "-lvorbisfile", "-lvorbis",
                     "-logg", "-lSDL2", "-lm", "-pthread", "-o",
                     str(staged_binary)])
            shutil.copyfile(staged_binary, binary)
            binary.chmod(0o755)
    report = {
        "schema": 1, "level": "COMPLETE EFFECTIVE ORIGINAL-GAME HOST LINK",
        "profile": args.profile, "compile_flags": flags,
        "okgf_build_type": "Release" if args.profile == "optimized" else "Debug",
        "okgf_library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
        "git_head": build_git, "git_dirty": dirty,
        "original_translation_units": len(list((GAME / "src").glob("*.cpp"))) + 1,
        "original_units_replaced": ["program.cpp", "EC_HsFile.cpp"],
        "compiled_effective_original_units": sum(1 for source, _ in mapping
                                                  if source.parent in (GAME / "src", GAME / "runtime")),
        "platform_units": len(PLATFORM_NAMES),
        "compiled_this_run": len(work), "linked_this_run": need_link,
        "link_objects": len(objects),
        "binary": str(binary), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "source_map": manifest,
    }
    (BUILD / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "source_map"}, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
