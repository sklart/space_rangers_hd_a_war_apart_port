"""Try the pinned full C++ game build without editing upstream/cpp."""

from __future__ import annotations

import argparse
import json
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from e2e_source_overrides import FUNCTIONS, generated_source
from e2e4_audio_on_overrides import AUDIO_ON_FUNCTIONS
from e2e_runtime_overrides import prepare_runtime_overlay
from e2e4_script_width_overrides import prepare_script_source_overlay
from e2e5_ui_width_overrides import apply as apply_ui_width_overrides
from e2e5_cachegi_trace import apply as apply_cachegi_trace

ROOT = Path(__file__).resolve().parents[1]
SWITCH = ROOT / "port/switch"
GAME = ROOT / "upstream/cpp"
BUILD = SWITCH / "build/e2e-game"
REPORT = ROOT / "e2e-build-report.json"
DEVKITPRO = Path(os.getenv("DEVKITPRO", "C:/devkitPro"))
if os.name == "nt" and not DEVKITPRO.is_dir():
    DEVKITPRO = Path("C:/devkitPro")
DEVKITA64 = Path(os.getenv("DEVKITA64", str(DEVKITPRO / "devkitA64")))
PORTLIBS = Path(os.getenv("PORTLIBS", str(DEVKITPRO / "portlibs/switch")))
if os.name == "nt" and not DEVKITA64.is_dir():
    DEVKITA64 = DEVKITPRO / "devkitA64"
if os.name == "nt" and not PORTLIBS.is_dir():
    PORTLIBS = DEVKITPRO / "portlibs/switch"
CXX = DEVKITA64 / "bin" / ("aarch64-none-elf-g++.exe" if os.name == "nt" else "aarch64-none-elf-g++")
MSYS = sys.platform.startswith(("cygwin", "msys"))


def native_path(path: Path) -> str:
    if MSYS:
        return subprocess.check_output(["cygpath", "-w", str(path)], text=True).strip().replace("\\", "/")
    return str(path)


def python_path(path: Path) -> str:
    # MSYS Python consumes POSIX paths; native devkitPro tools consume Win32
    # paths. Passing cygpath -w to sys.executable treats D: as a relative name.
    return str(path)


def cmake_path(path: Path) -> str:
    if os.name == "nt":
        return subprocess.check_output(["cygpath", "-u", str(path)], text=True).strip()
    return str(path)


def save(report: dict) -> None:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def run(command: list[str]) -> tuple[int, str]:
    env = os.environ.copy()
    env["DEVKITPRO"] = native_path(DEVKITPRO).replace("\\", "/")
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
    return result.returncode, (result.stdout + result.stderr).strip()


def dependency_paths(depfile: Path) -> list[Path] | None:
    """Read GCC -MMD output; missing or malformed state forces recompilation."""
    if not depfile.is_file():
        return None
    content = depfile.read_text(encoding="utf-8", errors="replace")
    content = content.replace("\\\r\n", " ").replace("\\\n", " ")
    marker = content.find(": ")
    if marker < 0:
        return None
    # GCC escapes spaces in prerequisites; drive-letter colons are not markers.
    names = re.split(r"(?<!\\)\s+", content[marker + 2:].strip())
    paths = [Path(name.replace("\\ ", " ")) for name in names if name]
    return paths or None


def object_current(obj: Path, depfile: Path, sources: list[Path], flags_stamp: Path) -> bool:
    if not obj.is_file() or not depfile.is_file():
        return False
    deps = dependency_paths(depfile)
    if deps is None:
        return False
    threshold = obj.stat().st_mtime_ns
    return all(path.is_file() and path.stat().st_mtime_ns <= threshold
               for path in [*sources, flags_stamp, *deps])


def prepare_patched_sources() -> dict[str, Path]:
    """Apply existing portability patches only inside a disposable build tree."""
    patch_root = BUILD / "patched-upstream"
    (patch_root / "src").mkdir(parents=True, exist_ok=True)
    names = ("GR_Main.cpp", "EC_BlockPar.cpp", "EC_Data.cpp")
    patches = ("gr_main_portable.patch", "gr_main_zlib_portable.patch",
               "dat_zlib_guard.patch")
    stamp = patch_root / "patch-inputs.json"
    inputs = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in [*(GAME / "src" / name for name in names),
                           *(SWITCH / "platform" / name for name in patches)]}
    expected = json.dumps(inputs, sort_keys=True, indent=2) + "\n"
    if (stamp.is_file() and stamp.read_text(encoding="utf-8") == expected and
            all((patch_root / "src" / name).is_file() for name in names)):
        return {name: patch_root / "src" / name for name in names}
    for name in names:
        shutil.copyfile(GAME / "src" / name, patch_root / "src" / name)
    native_git = Path("C:/Program Files/Git/cmd/git.exe")
    git = str(native_git) if native_git.is_file() else "git"
    for patch in patches:
        command = [git, "apply",
                   "--directory=" + patch_root.relative_to(ROOT).as_posix(),
                   "--ignore-space-change",
                   native_path(SWITCH / "platform" / patch)]
        code, output = run(command)
        if code:
            raise RuntimeError(f"disposable upstream patch failed: {patch}: {output}")
    if "portable user root is unavailable" not in (patch_root / "src/GR_Main.cpp").read_text(encoding="utf-8"):
        raise RuntimeError("disposable GR_Main portability patch was not applied")
    stamp.write_text(expected, encoding="utf-8")
    return {name: patch_root / "src" / name for name in names}


def build_okgf() -> tuple[Path, Path]:
    """Build the existing no-TLS Switch OKGF backend in the disposable tree."""
    build = BUILD / "okgf-no-tls"
    library = build / "libokgf.a"
    softfloat = build / "vendor/softfloat/libokgf_softfloat.a"
    if not library.is_file() or not softfloat.is_file():
        command = ["cmake", "-S", cmake_path(ROOT / "upstream/okgf"),
                   "-B", cmake_path(build), "-G", "Unix Makefiles",
                   "-DCMAKE_SYSTEM_NAME=Generic",
                   "-DCMAKE_C_COMPILER=" + cmake_path(DEVKITA64 / "bin/aarch64-none-elf-gcc"),
                   "-DCMAKE_C_FLAGS=-D_Thread_local=",
                   "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
                   "-DCMAKE_FIND_ROOT_PATH=" + cmake_path(PORTLIBS),
                   "-DCMAKE_PREFIX_PATH=" + cmake_path(PORTLIBS),
                   "-DOKGF_MATH_BACKEND=COMPATIBLE", "-DOKGF_PORTABLE_MATH=ON"]
        code, output = run(command)
        if code:
            raise RuntimeError("OKGF CMake configure failed: " + output)
        code, output = run(["cmake", "--build", cmake_path(build), "--parallel", "2"])
        if code:
            raise RuntimeError("OKGF ARM64 build failed: " + output)
    readelf = DEVKITA64 / "bin" / ("aarch64-none-elf-readelf.exe" if os.name == "nt" else "aarch64-none-elf-readelf")
    code, symbols = run([str(readelf), "-sW", native_path(library), native_path(softfloat)])
    if code or any(" TLS " in line for line in symbols.splitlines()):
        raise RuntimeError("Switch OKGF libraries contain TLS symbols or could not be inspected")
    return library, softfloat


def main() -> int:
    global BUILD, REPORT
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=("debug", "release"), default="release")
    parser.add_argument("--e2e3-trace", action=argparse.BooleanOptionalAction,
                        default=True, help="log E2E-3 state changes without per-frame output")
    variants = parser.add_mutually_exclusive_group()
    variants.add_argument("--experimental-quicksave", action="store_true",
                        help="build isolated E2E-4 recoverable quicksave experiment")
    variants.add_argument("--experimental-audio-on", action="store_true",
                          help="build isolated E2E-4 SDL2 game-audio experiment")
    variants.add_argument("--experimental-e2e4-combined", action="store_true",
                          help="build one isolated quicksave, Audio ON, and script-width experiment")
    parser.add_argument("--preflight-only", action="store_true",
                        help="generate effective sources and check manifests before compilation")
    args = parser.parse_args()
    if args.experimental_e2e4_combined:
        BUILD = SWITCH / "build/e2e-game-combined-experiment"
        REPORT = ROOT / "e2e4-combined-build-report.json"
    elif args.experimental_quicksave:
        BUILD = SWITCH / "build/e2e-game-quicksave-experiment"
        REPORT = ROOT / "e2e4-quicksave-build-report.json"
    elif args.experimental_audio_on:
        BUILD = SWITCH / "build/e2e-game-audio-experiment"
        REPORT = ROOT / "e2e4-audio-build-report.json"
    if args.preflight_only:
        REPORT = BUILD / "preflight-report.json"
    if args.experimental_e2e4_combined:
        args.experimental_quicksave = True
        args.experimental_audio_on = True
    script_width_enabled = args.experimental_e2e4_combined
    sources = sorted((GAME / "src").glob("*.cpp"))
    sources.append(GAME / "runtime/runtime.cpp")
    report = {
        "target": "switch-e2e-game",
        "build_profile": args.profile,
        "e2e3_trace": args.e2e3_trace,
        "experimental_quicksave": args.experimental_quicksave,
        "experimental_audio_on": args.experimental_audio_on,
        "experimental_script_width": script_width_enabled,
        "experimental_e2e4_combined": args.experimental_e2e4_combined,
        "upstream_pin": "57fa689c630193a66fdea6ca4c79a188814991cd",
        "upstream_source_files_total": len(sources),
        "compiled": 0,
        "excluded": [],
        "excluded_required": 0,
        "link_objects": 0,
        "undefined_symbols": None,
        "elf": None,
        "required_game_symbols": {},
        "windows_only_apis_encountered": [],
        "portable_implementations_used": [],
        "optional_stubs": [
            "Windows single-instance OpenEvent lookup (CreateEvent uses real portable events)",
            "HKLM AVI registry write",
            "Wine detection",
            "Steam DLL, initialization, callback thread, shutdown",
            "MMSystem timer-resolution request",
            "DirectSound device enumeration and WinMM audio timers (Audio OFF only)",
            "Windows thread priority is recorded but OS scheduling priority is unchanged",
            "COM apartment cleanup",
            "GetModuleFileNameA branch (entrypoint sets game working directory)",
        ],
        "required_stubs": [],
        "arm64_link": "NOT REACHED",
        "first_failure": None,
    }
    native_git = Path("C:/Program Files/Git/cmd/git.exe")
    git = str(native_git) if native_git.is_file() else "git"
    code, build_git = run([git, "rev-parse", "HEAD"])
    if code or len(build_git) != 40:
        report["first_failure"] = {"phase": "build provenance", "exact_error": build_git}
        save(report)
        print(f"E2E build provenance failed: {build_git}", file=sys.stderr)
        return 1
    report["build_git"] = build_git
    code, status = run([git, "status", "--porcelain", "--untracked-files=all"])
    if code:
        report["first_failure"] = {"phase": "build provenance", "exact_error": status}
        save(report)
        print(f"E2E build status failed: {status}", file=sys.stderr)
        return 1
    dirty_sources = [line for line in status.splitlines()
                     if not line.endswith(" e2e-build-report.json")]
    report["build_dirty"] = bool(dirty_sources)
    build_id = build_git[:7] + ("-dirty" if dirty_sources else "")
    report["source_overrides"] = sorted([*FUNCTIONS, "Rangers.cpp", "Globals.cpp",
                                          "GI_MessageLoop.cpp", "GR_Main.cpp", "GR_GraphBuf.cpp",
                                          "aSaveLoad.cpp", "aScript.cpp", "program.cpp"])
    if args.experimental_audio_on:
        report["source_overrides"] = sorted(set(report["source_overrides"]) |
                                            set(AUDIO_ON_FUNCTIONS))
    if args.experimental_e2e4_combined:
        report["source_overrides"] = sorted(set(report["source_overrides"]) |
                                            {"fHangar.cpp", "fRuinsTalk.cpp", "fTalk.cpp"})
    # These were created for the old diagnostic slice. They define symbols
    # already present in the complete original units and cannot be linked
    # alongside them; their replacement behavior must move behind original APIs.
    diagnostic_shims = {
        "runtime_failure_hooks_portable.cpp",
        "runtime_time.cpp", "sysutils_imports_portable.cpp",
        "windows_imports_portable.cpp",
    }
    report["diagnostic_shims_not_linked"] = sorted(diagnostic_shims)
    replaced_upstream = {
        "EC_HsFile.cpp": "original EC_HsFile API backed by portable ec_file_adapter.cpp",
    }
    for name, expected in (("cpp", "57fa689c630193a66fdea6ca4c79a188814991cd"),
                           ("okgf", "c01aa7a168a6f1772541501074b7bba1b96550ef")):
        submodule = ROOT / "upstream" / name
        code, actual = run([git, "-C", native_path(submodule), "rev-parse", "HEAD"])
        dirty_code, dirty = run([git, "-C", native_path(submodule), "status", "--porcelain"])
        if code or dirty_code or actual != expected or dirty:
            report["first_failure"] = {"phase": "preflight upstream pin",
                "exact_error": f"{name}: expected {expected}, actual {actual}, dirty={dirty}"}
            save(report)
            return 1
    tool_suffix = ".exe" if os.name == "nt" else ""
    required_toolchain = (CXX, DEVKITPRO / "tools/bin" / ("nacptool" + tool_suffix),
                          DEVKITPRO / "tools/bin" / ("elf2nro" + tool_suffix),
                          DEVKITPRO / "libnx/include/switch.h",
                          PORTLIBS / "include/SDL2/SDL.h",
                          PORTLIBS / "lib/libSDL2.a")
    missing_toolchain = [str(path) for path in required_toolchain if not path.is_file()]
    if missing_toolchain:
        report["first_failure"] = {"phase": "preflight toolchain",
                                   "exact_error": "missing: " + ", ".join(missing_toolchain)}
        save(report)
        print(f"E2E toolchain missing: {missing_toolchain}", file=sys.stderr)
        return 1
    patched_sources = prepare_patched_sources()
    script_source_root = (prepare_script_source_overlay(GAME / "src", BUILD / "source-overlay")
                          if script_width_enabled else None)
    runtime_overlay = BUILD / "runtime-overlay"
    prepare_runtime_overlay(GAME / "runtime", runtime_overlay)
    code, output = run([sys.executable,
                        python_path(ROOT / "tools/audit_win32_surface.py"), "--check"])
    if code:
        report["first_failure"] = {"phase": "Win32 source inventory", "exact_error": output}
        save(report)
        return 1
    manifest_path = ROOT / ("win32-effective-combined-import-manifest.json"
                            if args.experimental_e2e4_combined
                            else "win32-effective-import-manifest.json")
    if not manifest_path.is_file():
        report["first_failure"] = {"phase": "Win32 manifest stamp",
                                   "exact_error": "effective manifest is missing"}
        save(report)
        return 1
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    counts = manifest["classification_counts"]
    manifest_stamp = {
        "sha256": hashlib.sha256(manifest_bytes).hexdigest().upper(),
        "portable": counts.get("PORTABLE", 0),
        "optional": counts.get("OPTIONAL_DISABLED", 0),
        "unknown": counts.get("UNKNOWN", 0),
    }
    code, output = run([sys.executable,
                        python_path(ROOT / "tools/generate_okgf_compat_exports.py")])
    if code:
        report["first_failure"] = {"phase": "OKGF export table", "exact_error": output}
        save(report)
        return 1
    report["source_overrides"] += sorted(patched_sources)
    if script_source_root:
        report["source_overrides"] = sorted(set(report["source_overrides"]) |
                                            {"aScriptFun.cpp", "types/aScript.hpp",
                                             "EC_Expression.cpp", "types/EC_Expression.hpp"})
    # Generate the exact sources first. The Win32 manifest must be checked
    # against these files before the expensive ARM64 compile/link loop.
    effective_sources: dict[Path, Path] = {}
    source_map: dict[str, str] = {}
    for source in sources:
        effective_source = patched_sources.get(source.name,
            script_source_root / source.name if script_source_root and source.parent == GAME / "src"
            else source)
        compile_source = generated_source(effective_source,
                                          BUILD / "generated" / source.name,
                                          build_id, manifest_stamp,
                                          audio_on=args.experimental_audio_on,
                                          e2e5_profile=args.experimental_e2e4_combined,
                                          script_width=script_width_enabled)
        if script_source_root and source.parent == GAME / "src":
            current = compile_source.read_text(encoding="utf-8")
            repaired = apply_ui_width_overrides(source.name, current)
            if args.experimental_e2e4_combined and source.name == "EC_CacheGI.cpp":
                repaired = apply_cachegi_trace(repaired)
            if repaired != current:
                ui_source = BUILD / "generated-ui" / source.name
                ui_source.parent.mkdir(parents=True, exist_ok=True)
                if not ui_source.is_file() or ui_source.read_text(encoding="utf-8") != repaired:
                    ui_source.write_text(repaired, encoding="utf-8")
                compile_source = ui_source
        effective_sources[source] = compile_source
        source_map[source.relative_to(ROOT).as_posix()] = compile_source.relative_to(ROOT).as_posix()
    map_path = BUILD / "effective-source-map.json"
    if (len(sources) != 298 or len(source_map) != len(sources) or
            len(set(source_map.values())) != len(sources)):
        report["first_failure"] = {"phase": "preflight build graph",
                                   "exact_error": "effective source graph is incomplete or ambiguous"}
        save(report)
        return 1
    map_path.write_text(json.dumps(source_map, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    audit_command = [sys.executable, python_path(ROOT / "tools/audit_win32_surface.py"),
                     "--effective", "--build-root", native_path(BUILD),
                     "--manifest", native_path(manifest_path), "--check"]
    code, audit_output = run(audit_command)
    if code:
        report["first_failure"] = {"phase": "preflight Win32 effective manifest",
                                   "exact_error": audit_output}
        save(report)
        print(f"E2E preflight failed: {audit_output}", file=sys.stderr)
        return 1
    report["preflight"] = "PASS"
    if args.preflight_only:
        save(report)
        print(f"E2E preflight PASS: {manifest_path.name}")
        return 0
    flags = ["-std=gnu++20", "-D__SWITCH__", "-march=armv8-a+crc+crypto",
             "-mtune=cortex-a57", "-mtp=soft", "-fPIE",
             "-ffunction-sections", "-fdata-sections"]
    if args.profile == "release":
        flags += ["-O2", "-fno-strict-aliasing", "-DE2E_DIAGNOSTICS=0"]
    else:
        flags += ["-O0", "-DE2E_DIAGNOSTICS=1"]
    flags += ["-DE2E3_TRACE=" + str(int(args.e2e3_trace))]
    if args.experimental_quicksave:
        flags += ["-DE2E4_QUICKSAVE_EXPERIMENT=1"]
    if args.experimental_e2e4_combined:
        flags += ["-DE2E5_DISABLE_DISK_SAVE=1"]
    if args.experimental_audio_on:
        flags += ["-DE2E4_AUDIO_ON=1"]
    include_roots = [DEVKITPRO / "libnx/include", PORTLIBS / "include",
                     runtime_overlay, SWITCH / "platform"]
    if script_source_root:
        include_roots.append(script_source_root)
    include_roots += [GAME / "src", GAME / "runtime", ROOT / "upstream/okgf/include"]
    flags += ["-I" + str(p) for p in include_roots]
    flags = [flag if not flag.startswith("-I") else "-I" + native_path(Path(flag[2:])) for flag in flags]
    compiler_code, compiler_version = run([str(CXX), "--version"])
    if compiler_code:
        report["first_failure"] = {"phase": "toolchain", "exact_error": compiler_version}
        save(report)
        return 1
    fingerprints = {
        "compiler": str(CXX.resolve()),
        "compiler_version": compiler_version.splitlines()[0],
        "flags": flags,
        # Generated source content and actual included headers are checked per
        # object below.  A change in one override no longer invalidates 298
        # unrelated original translation units.
    }
    flags_stamp = BUILD / "compile-flags.txt"
    rendered_flags = json.dumps(fingerprints, sort_keys=True, indent=2) + "\n"
    if not flags_stamp.exists() or flags_stamp.read_text(encoding="utf-8") != rendered_flags:
        flags_stamp.write_text(rendered_flags, encoding="utf-8")
    report["game_compile_optimization"] = ("-O2 -fno-strict-aliasing" if args.profile == "release" else "-O0")
    report["compile_fingerprint_sha256"] = hashlib.sha256(rendered_flags.encode()).hexdigest().upper()
    objects: list[Path] = []
    reused_objects = 0
    compiled_objects = 0
    platform_sources = sorted((SWITCH / "platform").glob("*.cpp"))
    if not args.experimental_quicksave:
        platform_sources = [source for source in platform_sources
                            if source.name != "e2e4_quicksave_transaction.cpp"]
    if not args.experimental_audio_on:
        platform_sources = [source for source in platform_sources
                            if source.name != "e2e4_game_audio_bridge.cpp"]
    for source in sources + platform_sources:
        category = "upstream" if source in sources else "platform"
        if category == "platform" and source.name in diagnostic_shims:
            continue
        obj = BUILD / category / (source.stem + ".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        compile_source = effective_sources[source] if category == "upstream" else source
        depfile = obj.with_suffix(".d")
        if object_current(obj, depfile, [compile_source], flags_stamp):
            code, output = 0, ""
            reused_objects += 1
        else:
            code, output = run([str(CXX), *flags, "-MMD", "-MF", native_path(depfile),
                                "-c", native_path(compile_source), "-o", native_path(obj)])
            compiled_objects += 1
        if code:
            report["first_failure"] = {
                "phase": "compiler", "file": source.relative_to(ROOT).as_posix(),
                "exact_error": output,
            }
            save(report)
            print(f"E2E compiler failure in {source.relative_to(ROOT)}:\n{output}", file=sys.stderr)
            return 1
        if category == "upstream" and source.name in replaced_upstream:
            report["excluded"].append({"file": source.relative_to(ROOT).as_posix(),
                                       "reason": replaced_upstream[source.name]})
        else:
            objects.append(obj)
        if category == "upstream":
            report["compiled"] += 1
    report["link_objects"] = len(objects)
    report["compiled_objects_this_run"] = compiled_objects
    report["reused_objects_this_run"] = reused_objects
    report["portable_implementations_used"] = [
        obj.stem for obj in objects if obj.parent.name == "platform"
    ]
    try:
        okgf_library, okgf_softfloat = build_okgf()
    except RuntimeError as error:
        report["first_failure"] = {"phase": "OKGF ARM64 build", "exact_error": str(error)}
        save(report)
        print(error, file=sys.stderr)
        return 1
    report["portable_implementations_used"].append("okgf_no_tls")
    artifact_stem = ("SpaceRangersHDAWarApartE2E4Combined" if args.experimental_e2e4_combined
                     else "SpaceRangersHDAWarApartE2EAudioON" if args.experimental_audio_on
                     else "SpaceRangersHDAWarApartE2E")
    elf = BUILD / f"{artifact_stem}.elf"
    response_file = BUILD / "link-objects.rsp"
    response_file.write_text("\n".join('"' + native_path(obj) + '"' for obj in objects) + "\n", encoding="utf-8")
    code, output = run([str(CXX), "@" + native_path(response_file),
        "-specs=" + native_path(DEVKITPRO / "libnx/switch.specs"),
        "-march=armv8-a+crc+crypto", "-mtp=soft", "-fPIE",
        "-Wl,--gc-sections", "-L" + native_path(DEVKITPRO / "libnx/lib"),
        "-L" + native_path(PORTLIBS / "lib"),
        native_path(okgf_library), native_path(okgf_softfloat),
        "-lpng", "-lz", "-ljpeg", "-lvorbisfile", "-lvorbis", "-logg",
        "-lm", "-lSDL2", "-lEGL",
        "-lglapi", "-ldrm_nouveau", "-lnx", "-lpthread", "-o", native_path(elf)])
    if code:
        report["first_failure"] = {"phase": "linker", "exact_error": output}
        report["arm64_link"] = "FAIL"
        save(report)
        print(f"E2E ARM64 linker failure:\n{output}", file=sys.stderr)
        return 1
    report["arm64_link"] = "PASS"
    report["undefined_symbols"] = 0
    readelf = DEVKITA64 / "bin" / ("aarch64-none-elf-readelf.exe" if os.name == "nt" else "aarch64-none-elf-readelf")
    code, elf_header = run([str(readelf), "-h", native_path(elf)])
    if code or "ELF64" not in elf_header or "AArch64" not in elf_header:
        report["first_failure"] = {"phase": "ELF audit", "exact_error": elf_header}
        save(report)
        return 1
    report["elf"] = "ELF64 AArch64 PIE"
    nm = DEVKITA64 / "bin" / ("aarch64-none-elf-nm.exe" if os.name == "nt" else "aarch64-none-elf-nm")
    code, symbols = run([str(nm), "-C", native_path(elf)])
    required = ("Rangers::ProgramMain()", "Globals::RunMainScreenStateLoop()")
    report["required_game_symbols"] = {name: name in symbols for name in required}
    if code or not all(report["required_game_symbols"].values()):
        report["first_failure"] = {"phase": "game symbol audit", "exact_error":
                                   f"required original game symbols missing: {report['required_game_symbols']}"}
        save(report)
        return 1
    code, audit_output = run(audit_command)
    if manifest_path.is_file():
        manifest_bytes = manifest_path.read_bytes()
        manifest = json.loads(manifest_bytes)
        report["win32_surface"] = {
            "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest().upper(),
            "imports": len(manifest["imports"]),
            "dynamic_calls": len(manifest["dynamic_calls"]),
            "classifications": manifest["classification_counts"],
            "startup_unimplemented": manifest["startup_unimplemented"],
        }
        if report["win32_surface"]["manifest_sha256"] != manifest_stamp["sha256"]:
            code = 1
            audit_output += "\nmanifest changed after startup stamp generation"
    if code:
        report["first_failure"] = {"phase": "Win32 compatibility surface",
                                   "exact_error": audit_output}
        save(report)
        print(f"E2E Win32 compatibility gate: {audit_output}", file=sys.stderr)
        return 1
    for gate, gate_args in (("portable import mapping",
                        ["tools/generate_win32_portable_cases.py", "--check"]),
                       ("synthetic handle width",
                        ["tools/audit_win32_handle_width.py"])):
        code, gate_output = run([sys.executable, python_path(ROOT / gate_args[0]),
                                 *gate_args[1:]])
        if code:
            report["first_failure"] = {"phase": gate, "exact_error": gate_output}
            save(report)
            print(f"E2E {gate} gate: {gate_output}", file=sys.stderr)
            return 1
    tools_bin = DEVKITPRO / "tools/bin"
    suffix = ".exe" if os.name == "nt" else ""
    nacp = BUILD / f"{artifact_stem}.nacp"
    nro = BUILD / f"{artifact_stem}.nro"
    code, output = run([str(tools_bin / ("nacptool" + suffix)), "--create",
                        "Space Rangers HD: A War Apart E2E-4 Combined" if args.experimental_e2e4_combined
                        else "Space Rangers HD: A War Apart E2E Audio ON" if args.experimental_audio_on
                        else "Space Rangers HD: A War Apart E2E", "sklart", "0.0.1", native_path(nacp)])
    if not code:
        code, output = run([str(tools_bin / ("elf2nro" + suffix)), native_path(elf),
                            native_path(nro), "--nacp=" + native_path(nacp),
                            "--icon=" + native_path(SWITCH / "assets/icon.jpg")])
    if code or not nro.is_file():
        report["first_failure"] = {"phase": "NRO packaging", "exact_error": output}
        save(report)
        return 1
    strings_tool = DEVKITA64 / "bin" / ("aarch64-none-elf-strings.exe" if os.name == "nt" else "aarch64-none-elf-strings")
    code, strings_output = run([str(strings_tool), native_path(elf)])
    if code:
        report["first_failure"] = {"phase": "symbol audit", "exact_error": strings_output}
        save(report)
        return 1
    report["windows_only_apis_encountered"] = sorted({
        line.strip().lower() for line in strings_output.splitlines()
        if re.fullmatch(r"[a-z0-9_.-]+\.dll", line.strip(), re.IGNORECASE)
    })
    nro_bytes = nro.read_bytes()
    # A dirty developer build may change source without a new Git commit.
    # Keep each distinct output immutable, while clean CI names stay tied to
    # the exact commit.
    content_suffix = "-" + hashlib.sha256(nro_bytes).hexdigest()[:12] if dirty_sources else ""
    candidate = BUILD / f"{artifact_stem}-{build_id}{content_suffix}.nro"
    if candidate.exists():
        if candidate.read_bytes() != nro_bytes:
            report["first_failure"] = {
                "phase": "immutable NRO", "exact_error": f"candidate differs: {candidate}"
            }
            save(report)
            return 1
    else:
        candidate.write_bytes(nro_bytes)
    elf_bytes = elf.read_bytes()
    matching_elf = BUILD / f"{artifact_stem}-{build_id}{content_suffix}.elf"
    if matching_elf.exists():
        if matching_elf.read_bytes() != elf_bytes:
            report["first_failure"] = {
                "phase": "immutable ELF", "exact_error": f"matching ELF differs: {matching_elf}"
            }
            save(report)
            return 1
    else:
        matching_elf.write_bytes(elf_bytes)
    report["matching_elf"] = {"path": matching_elf.relative_to(ROOT).as_posix(),
                              "bytes": len(elf_bytes),
                              "sha256": hashlib.sha256(elf_bytes).hexdigest().upper()}
    report["nro"] = {"path": candidate.relative_to(ROOT).as_posix(),
                     "working_path": nro.relative_to(ROOT).as_posix(),
                     "bytes": len(nro_bytes),
                     "sha256": hashlib.sha256(nro_bytes).hexdigest().upper(),
                     "status": "SOFTWARE AUDIT PASS; hardware not tested"}
    save(report)
    print(f"E2E ARM64 ELF: {elf}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
