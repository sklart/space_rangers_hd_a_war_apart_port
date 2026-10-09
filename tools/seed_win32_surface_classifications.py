"""Seed only entries whose Switch contract has been verified in source."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
inventory = json.loads((ROOT / "win32-import-manifest.json").read_text(encoding="utf-8"))
path = ROOT / "tools/win32_surface_classifications.json"
data = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {
    "imports": {}, "dynamic": {}, "direct": {}}

def classified(kind, implementation, reason, startup=False, gameplay=False):
    return {"classification": kind, "implementation": implementation,
            "reason": reason, "startup_required": startup,
            "gameplay_required": gameplay}

known = {
    "kernel32.dll": {
        "win32_compat_files": "CreateFileA CreateFileW ReadFile WriteFile SetFilePointer GetFileSize GetFileAttributesA DeleteFileA FindFirstFileA FindFirstFileW FindNextFileA FindNextFileW FindClose CreateDirectoryA GetCurrentDirectoryA SetCurrentDirectoryA CopyFileA CopyFileW MoveFileW SetFileAttributesA GetVolumeInformationA",
        "win32_compat_sync": "CreateEventA CreateThread SetEvent ResetEvent WaitForSingleObject WaitForMultipleObjects CloseHandle ResumeThread GetCurrentThread GetCurrentThreadId SetThreadPriority GetThreadPriority OpenEventA TerminateThread",
        "e2e_calendar": "GetSystemTime GetLocalTime",
        "e2e_clock": "QueryPerformanceCounter QueryPerformanceFrequency GetTickCount",
        "win32_handles": "GetLastError",
        "win32_compat_modules": "LoadLibraryA LoadLibraryW GetModuleHandleA GetModuleHandleW GetProcAddress FreeLibrary",
        "win32_compat_process": "GetCommandLineA GetModuleFileNameA GetCurrentProcess GlobalAlloc GlobalLock GlobalUnlock GlobalMemoryStatus GlobalMemoryStatusEx GetSystemInfo SetPriorityClass GetPriorityClass Sleep GetVersion GetVersionExA",
        "win32_compat_filetime": "CompareFileTime FileTimeToSystemTime FileTimeToLocalFileTime FileTimeToDosDateTime",
        "win32_compat_text": "CompareStringA FormatMessageA",
        "win32_compat_heap": "HeapCreate HeapDestroy",
    },
    "zlib.dll": {"zlib_bridge": "OKGF_ZLib_Compress OKGF_ZLib_UnCompress OKGF_ZLib_UnCompress2"},
    "advapi32.dll": {"win32_compat_registry": "RegCloseKey RegCreateKeyExW RegFlushKey RegOpenKeyExA RegQueryValueExA RegSetValueExW"},
    "gdi32.dll": {"win32_compat_gdi": "CreateDIBSection DeleteObject GetStockObject"},
    "user32.dll": {
        "win32_compat_window": "GetActiveWindow CreateWindowExW RegisterClassW DestroyWindow ShowWindow SetWindowPos SetFocus GetForegroundWindow SetForegroundWindow UpdateWindow RedrawWindow SetWindowTextA AdjustWindowRect ClientToScreen ScreenToClient IntersectRect UnionRect SetWindowLongA GetDC ReleaseDC GetDoubleClickTime",
        "win32_compat_messages": "PostMessageA PeekMessageA PeekMessageW TranslateMessage DispatchMessageW DefWindowProcW PostQuitMessage RegisterWindowMessageA SetTimer MsgWaitForMultipleObjects GetAsyncKeyState TrackMouseEvent",
        "win32_compat_gdi": "LoadCursorA LoadIconA CreateIconIndirect DestroyIcon SetCursor ShowCursor SetCursorPos GetCursorPos ClipCursor",
        "win32_compat_text": "CharNextA CharLowerBuffA CharLowerBuffW CharUpperBuffW MessageBoxA",
        "win32_compat_clipboard": "OpenClipboard CloseClipboard EmptyClipboard SetClipboardData GetClipboardData",
    },
}
for item in inventory["imports"]:
    dll, symbol = item["dll"], item["symbol"]
    key = dll + "::" + symbol
    if dll == "okgf.dll":
        data["imports"][key] = classified(
            "STATIC_LIBRARY", "win32_compat_okgf",
            "The generated export table takes the address of this symbol in linked OKGF.",
            gameplay=True)
    elif dll == "kernel32" and symbol == "GlobalMemoryStatusEx":
        data["imports"][key] = classified("PORTABLE", "win32_compat_process",
            "The resolver normalizes kernel32 and reads Switch process memory through svcGetInfo.",
            startup=True)
    elif dll == "kernel32.dll" and symbol in {"GetLocaleInfoA", "GetThreadLocale"}:
        data["imports"][key] = classified("OPTIONAL_DISABLED", "runtime/locale.hpp non-Windows branch",
            "This source import is inside _WIN32 and absent from effective Switch preprocessing.")
    elif dll == "kernel32.dll" and symbol == "SetProcessAffinityMask":
        data["imports"][key] = classified("OPTIONAL_DISABLED", "GR_Main Switch source adaptation",
            "The CPU-affinity probe is omitted on Switch; resolver reports access denied if called.")
    elif dll == "kernel32.dll" and symbol == "GetSystemDirectoryW":
        data["imports"][key] = classified("OPTIONAL_DISABLED", "GR_Main Switch source adaptation",
            "The Windows DirectX system-directory loader path is omitted; resolver reports path not found.")
    elif dll in known:
        for implementation, symbols in known[dll].items():
            if symbol in symbols.split():
                data["imports"][key] = classified(
                    "PORTABLE", implementation,
                    "Typed resolver thunk is present; functional semantics still require group tests.",
                    startup=symbol in {"CreateEventA", "CreateThread", "SetEvent",
                        "ResetEvent", "WaitForSingleObject", "WaitForMultipleObjects",
                        "CloseHandle", "ResumeThread", "GetSystemTime", "GetLocalTime",
                        "QueryPerformanceCounter", "QueryPerformanceFrequency", "GetTickCount",
                        "GetLastError", "OpenEventA", "GetCurrentThreadId", "LoadLibraryA",
                        "LoadLibraryW", "GetProcAddress", "FreeLibrary"},
                    gameplay=True)
                break
    elif dll == "dsound.dll":
        data["imports"][key] = classified("OPTIONAL_DISABLED", "audio-off policy",
            "GR_Main source adaptation disables sound and music for E2E-1.", gameplay=True)
    elif dll == "avifil32.dll":
        if symbol == "AVIFileExit":
            data["imports"][key] = classified("PORTABLE", "win32_compat empty AVI cleanup",
                "TxvidGI::XvidClose calls AVIFileExit while destroying never-opened video objects during global UI startup; no AVI session exists on Switch.",
                startup=True, gameplay=False)
        else:
            data["imports"][key] = classified("OPTIONAL_DISABLED", "video-off policy",
                "VFW calls originate in GI_XviD intro video, which is disabled for E2E-1.", gameplay=False)
    elif dll == "gdiplus.dll":
        data["imports"][key] = classified("NOT_REACHED_UNTIL_GAMEPLAY", "graphics.hpp screenshot path",
            "The GDI+ API object is constructed only for bitmap/JPEG screenshot work in GR_GraphBuf.",
            gameplay=True)
    elif dll == "ole32.dll" and symbol == "CreateStreamOnHGlobal":
        data["imports"][key] = classified("NOT_REACHED_UNTIL_GAMEPLAY", "graphics.hpp screenshot path",
            "COM memory stream is used only by JPEG screenshot encoding.", gameplay=True)
    elif dll == "ole32.dll":
        data["imports"][key] = classified("OPTIONAL_DISABLED", "GR_Main Switch source adaptation",
            "COM init/cleanup or the desktop documents fallback is bypassed by portable platform and user root.")
    elif dll == "shell32.dll":
        data["imports"][key] = classified("OPTIONAL_DISABLED", "portable user root / desktop help",
            "Desktop Documents lookup is bypassed by portable user root; help-file ShellExecute has no E2E menu path.")
    elif dll == "winmm.dll" and symbol in {"timeSetEvent", "timeKillEvent"}:
        data["imports"][key] = classified("OPTIONAL_DISABLED", "audio-off policy",
            "Audio timer path is disabled for E2E-1.")
    elif dll == "winmm.dll" and symbol in {"timeGetTime", "timeBeginPeriod", "timeEndPeriod"}:
        data["imports"][key] = classified("PORTABLE", "win32_compat_winmm",
            "Original MMSystem wrapper resolves a portable monotonic clock or timer-period policy.")

for item in inventory["direct_declarations"]:
    key = item["dll"] + "::" + item["symbol"]
    data["direct"][key] = classified(
        "PORTABLE", "runtime non-Windows branch",
        "Declaration is inside _WIN32; Switch compiles the adjacent portable runtime branch.")

for item in inventory["dynamic_calls"]:
    source = Path(item["callsite"].split(":")[0]).name
    key = source + "::" + item["api"] + "::" + item["expression"]
    if source in {"SimpleSteamApi.cpp", "VorbisFile.cpp", "GI_XviD.cpp",
                  "GR_Music.cpp", "Rangers.cpp"}:
        domain = ("Steam" if source == "SimpleSteamApi.cpp" else
                  "Wine detection" if source == "Rangers.cpp" else
                  "audio" if source in {"VorbisFile.cpp", "GR_Music.cpp"} else "intro video")
        data["dynamic"][key] = classified(
            "OPTIONAL_DISABLED", "win32_compat_modules",
            f"{domain} is disabled in E2E-1; module loader returns unavailable.")
    elif source == "Robot.cpp":
        data["dynamic"][key] = classified(
            "RUNTIME_MODULE", "win32_compat_modules",
            "Robot init accepts a missing MatrixGame module; robot battle gameplay then lacks its interface.",
            gameplay=True)
    elif source in {"aScript.cpp", "EC_Expression.cpp"}:
        data["dynamic"][key] = classified(
            "RUNTIME_MODULE", "win32_compat_modules",
            "Script-supplied module/export is validated by the runtime module registry; unknown loads fail loudly.",
            gameplay=True)
    elif source == "aGalaxy.cpp":
        data["dynamic"][key] = classified(
            "NOT_REACHED_UNTIL_GAMEPLAY", "win32_compat_modules",
            "Galaxy construction/integrity check probes decoded Windows module names after a game starts.",
            gameplay=True)
    elif source == "GR_Main.cpp":
        data["dynamic"][key] = classified(
            "OPTIONAL_DISABLED", "GR_Main Switch source adaptation",
            "Windows DirectX/WOW64/integrity path is bypassed by generated Switch GR_Main source.")

path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
print(f"seeded {len(data['imports'])} static imports and {len(data['direct'])} direct declarations")
