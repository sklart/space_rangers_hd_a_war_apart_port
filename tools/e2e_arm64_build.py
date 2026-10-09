"""Try the pinned full C++ game build without editing upstream/cpp."""

from __future__ import annotations

import json
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from e2e_source_overrides import FUNCTIONS, generated_source
from e2e_runtime_overrides import prepare_runtime_overlay

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


def cmake_path(path: Path) -> str:
    if os.name == "nt":
        return subprocess.check_output(["cygpath", "-u", str(path)], text=True).strip()
    return str(path)


def save(report: dict) -> None:
    REPORT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def run(command: list[str]) -> tuple[int, str]:
    env = os.environ.copy()
    env["DEVKITPRO"] = native_path(DEVKITPRO).replace("\\", "/")
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
    return result.returncode, (result.stdout + result.stderr).strip()


def prepare_patched_sources() -> dict[str, Path]:
    """Apply existing portability patches only inside a disposable build tree."""
    patch_root = BUILD / "patched-upstream"
    (patch_root / "src").mkdir(parents=True, exist_ok=True)
    names = ("GR_Main.cpp", "EC_BlockPar.cpp", "EC_Data.cpp")
    for name in names:
        shutil.copyfile(GAME / "src" / name, patch_root / "src" / name)
    native_git = Path("C:/Program Files/Git/cmd/git.exe")
    git = str(native_git) if native_git.is_file() else "git"
    for patch in ("gr_main_portable.patch", "gr_main_zlib_portable.patch",
                  "dat_zlib_guard.patch"):
        command = [git, "apply",
                   "--directory=" + patch_root.relative_to(ROOT).as_posix(),
                   "--ignore-space-change",
                   native_path(SWITCH / "platform" / patch)]
        code, output = run(command)
        if code:
            raise RuntimeError(f"disposable upstream patch failed: {patch}: {output}")
    if "portable user root is unavailable" not in (patch_root / "src/GR_Main.cpp").read_text(encoding="utf-8"):
        raise RuntimeError("disposable GR_Main portability patch was not applied")
    return {name: patch_root / "src" / name for name in names}


def build_okgf() -> tuple[Path, Path]:
    """Build the existing no-TLS Switch OKGF backend in the disposable tree."""
    build = BUILD / "okgf-no-tls"
    library = build / "libokgf.a"
    softfloat = build / "vendor/softfloat/libokgf_softfloat.a"
    if not library.is_file() or not softfloat.is_file():
        command = ["cmake", "-S", native_path(ROOT / "upstream/okgf"),
                   "-B", native_path(build), "-G", "Unix Makefiles",
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
        code, output = run(["cmake", "--build", native_path(build), "--parallel", "2"])
        if code:
            raise RuntimeError("OKGF ARM64 build failed: " + output)
    readelf = DEVKITA64 / "bin" / ("aarch64-none-elf-readelf.exe" if os.name == "nt" else "aarch64-none-elf-readelf")
    code, symbols = run([str(readelf), "-sW", native_path(library), native_path(softfloat)])
    if code or any(" TLS " in line for line in symbols.splitlines()):
        raise RuntimeError("Switch OKGF libraries contain TLS symbols or could not be inspected")
    return library, softfloat


def main() -> int:
    sources = sorted((GAME / "src").glob("*.cpp"))
    sources.append(GAME / "runtime/runtime.cpp")
    report = {
        "target": "switch-e2e-game",
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
            "DirectSound device enumeration and WinMM audio timers (audio disabled)",
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
                                          "aSaveLoad.cpp", "program.cpp"])
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
    if not CXX.is_file():
        report["first_failure"] = {"phase": "toolchain", "error": f"missing {CXX}"}
        save(report)
        print(f"E2E toolchain missing: {CXX}", file=sys.stderr)
        return 1
    patched_sources = prepare_patched_sources()
    runtime_overlay = BUILD / "runtime-overlay"
    prepare_runtime_overlay(GAME / "runtime", runtime_overlay)
    code, output = run([sys.executable,
                        native_path(ROOT / "tools/audit_win32_surface.py"), "--check"])
    if code:
        report["first_failure"] = {"phase": "Win32 source inventory", "exact_error": output}
        save(report)
        return 1
    manifest_path = ROOT / "win32-effective-import-manifest.json"
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
                        native_path(ROOT / "tools/generate_okgf_compat_exports.py")])
    if code:
        report["first_failure"] = {"phase": "OKGF export table", "exact_error": output}
        save(report)
        return 1
    report["source_overrides"] += sorted(patched_sources)
    flags = ["-std=gnu++20", "-D__SWITCH__", "-march=armv8-a+crc+crypto",
             "-mtune=cortex-a57", "-mtp=soft", "-fPIE", "-O0",
             "-ffunction-sections", "-fdata-sections"]
    flags += ["-I" + str(p) for p in (
        DEVKITPRO / "libnx/include", PORTLIBS / "include", runtime_overlay,
        SWITCH / "platform",
        GAME / "src", GAME / "runtime", ROOT / "upstream/okgf/include")]
    flags = [flag if not flag.startswith("-I") else "-I" + native_path(Path(flag[2:])) for flag in flags]
    headers = [*GAME.rglob("*.hpp"), *(SWITCH / "platform").glob("*.hpp"),
               *runtime_overlay.rglob("*.hpp")]
    newest_header_ns = max((path.stat().st_mtime_ns for path in headers), default=0)
    objects: list[Path] = []
    for source in sources + sorted((SWITCH / "platform").glob("*.cpp")):
        category = "upstream" if source in sources else "platform"
        if category == "platform" and source.name in diagnostic_shims:
            continue
        obj = BUILD / category / (source.stem + ".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        compile_source = generated_source(patched_sources.get(source.name, source),
                                          BUILD / "generated" / source.name,
                                          build_id, manifest_stamp)
        needed_ns = max(compile_source.stat().st_mtime_ns, newest_header_ns)
        if compile_source != source:
            needed_ns = max(needed_ns,
                (SWITCH / "platform/e2e_clock.hpp").stat().st_mtime_ns,
                (SWITCH / "platform/e2e_stage.hpp").stat().st_mtime_ns)
        if obj.is_file() and obj.stat().st_mtime_ns >= needed_ns:
            code, output = 0, ""
        else:
            code, output = run([str(CXX), *flags, "-c", native_path(compile_source), "-o", native_path(obj)])
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
    elf = BUILD / "SpaceRangersHDAWarApartE2E.elf"
    response_file = BUILD / "link-objects.rsp"
    response_file.write_text("\n".join('"' + native_path(obj) + '"' for obj in objects) + "\n", encoding="utf-8")
    code, output = run([str(CXX), "@" + native_path(response_file),
        "-specs=" + native_path(DEVKITPRO / "libnx/switch.specs"),
        "-march=armv8-a+crc+crypto", "-mtp=soft", "-fPIE",
        "-Wl,--gc-sections", "-L" + native_path(DEVKITPRO / "libnx/lib"),
        "-L" + native_path(PORTLIBS / "lib"),
        native_path(okgf_library), native_path(okgf_softfloat),
        "-lpng", "-lz", "-ljpeg", "-lm", "-lSDL2", "-lEGL",
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
    code, audit_output = run([sys.executable, native_path(ROOT / "tools/audit_win32_surface.py"),
                              "--effective", "--check"])
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
    for gate, args in (("portable import mapping",
                        ["tools/generate_win32_portable_cases.py", "--check"]),
                       ("synthetic handle width",
                        ["tools/audit_win32_handle_width.py"])):
        code, gate_output = run([sys.executable, native_path(ROOT / args[0]),
                                 *args[1:]])
        if code:
            report["first_failure"] = {"phase": gate, "exact_error": gate_output}
            save(report)
            print(f"E2E {gate} gate: {gate_output}", file=sys.stderr)
            return 1
    tools_bin = DEVKITPRO / "tools/bin"
    suffix = ".exe" if os.name == "nt" else ""
    nacp = BUILD / "SpaceRangersHDAWarApartE2E.nacp"
    nro = BUILD / "SpaceRangersHDAWarApartE2E.nro"
    code, output = run([str(tools_bin / ("nacptool" + suffix)), "--create",
                        "Space Rangers HD: A War Apart E2E", "sklart", "0.0.1", native_path(nacp)])
    if not code:
        code, output = run([str(tools_bin / ("elf2nro" + suffix)), native_path(elf),
                            native_path(nro), "--nacp=" + native_path(nacp),
                            "--icon=" + native_path(SWITCH / "assets/icon.jpg")])
    if code or not nro.is_file():
        report["first_failure"] = {"phase": "NRO packaging", "exact_error": output}
        save(report)
        return 1
    report["nro"] = {"path": nro.relative_to(ROOT).as_posix(),
                     "bytes": nro.stat().st_size,
                     "sha256": hashlib.sha256(nro.read_bytes()).hexdigest().upper(),
                     "status": "SOFTWARE AUDIT PASS; hardware not tested"}
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
    save(report)
    print(f"E2E ARM64 ELF: {elf}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
