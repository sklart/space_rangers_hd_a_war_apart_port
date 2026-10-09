"""Compile and run the Win32 compatibility tests through the game wrappers."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile

from e2e_runtime_overrides import prepare_runtime_overlay


ROOT = Path(__file__).resolve().parent.parent
PLATFORM = ROOT / "port/switch/platform"
RUNTIME = ROOT / "upstream/cpp/runtime"
GAME = ROOT / "upstream/cpp/src"
OVERLAY = ROOT / "port/switch/build/e2e-game/runtime-overlay"


def run(command: list[str]) -> None:
    completed = subprocess.run(command, cwd=ROOT, text=True)
    if completed.returncode:
        raise SystemExit(completed.returncode)


def main() -> None:
    OVERLAY.mkdir(parents=True, exist_ok=True)
    prepare_runtime_overlay(RUNTIME, OVERLAY)
    compiler = os.environ.get("CXX", "g++")
    common = [compiler, "-std=gnu++20", "-O0", "-include", "unistd.h",
              "-ffunction-sections", "-fdata-sections", "-DE2E_HOST_OKGF_STUB",
              "-I" + str(ROOT / "tests"), "-I" + str(PLATFORM),
              "-I" + str(OVERLAY), "-I" + str(GAME), "-I" + str(RUNTIME)]
    implementation = [str(source) for source in sorted(PLATFORM.glob("win32_compat*.cpp"))
                      if source.name != "win32_compat_okgf.cpp"]
    implementation += [str(PLATFORM / name) for name in (
        "win32_handles.cpp", "e2e_calendar.cpp", "e2e_events.cpp",
        "e2e_threads.cpp", "e2e_file_match.cpp", "game_path.cpp",
        "user_root.cpp", "input_platform.cpp", "zlib_bridge.cpp")]
    with tempfile.TemporaryDirectory(prefix="e2e-win32-host-") as temp:
        for name, source, units in (
            ("handles", "test_win32_handles.cpp", ["win32_handles.cpp"]),
            ("filetime", "test_win32_compat_filetime.cpp",
             ["win32_compat_filetime.cpp", "win32_handles.cpp",
              "e2e_calendar.cpp"]),
            ("window-message-input", "test_win32_compat_messages.cpp",
             ["win32_compat_messages.cpp", "win32_compat_window.cpp",
              "win32_handles.cpp", "e2e_events.cpp", "input_platform.cpp"]),
        ):
            output = Path(temp) / name
            run(common + [str(ROOT / "tests" / source),
                          *(str(PLATFORM / unit) for unit in units),
                          "-Wl,--gc-sections", "-o", str(output)])
            run([str(output)])
            print(f"{name}: PASS", flush=True)
        for name, source, wrappers in (
            ("resolver", ROOT / "tests/test_win32_compat_resolver.cpp", []),
            ("original-wrappers", ROOT / "tests/test_win32_original_wrappers.cpp",
             [GAME / "WindowsImports.cpp", GAME / "WindowsSdk.cpp",
              RUNTIME / "runtime.cpp"]),
        ):
            output = Path(temp) / name
            flags = ["-DE2E_HOST_RESOLVER_TEST"] if wrappers else []
            run(common + flags + [str(source), *map(str, wrappers), *implementation,
                                  "-Wl,--gc-sections", "-lz", "-o", str(output)])
            run([str(output)])
            print(f"{name}: PASS", flush=True)


if __name__ == "__main__":
    main()
