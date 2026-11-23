"""Read selected screen IDs from the live isolated Windows reference process."""
import ctypes
from ctypes import wintypes
import psutil

processes = [p for p in psutil.process_iter(['name']) if p.info['name'] == 'gov-frun-dispatcher.exe']
if len(processes) != 1:
    raise RuntimeError('expected exactly one live reference process')
process = processes[0]
regions = [int(item.addr.split('-')[0], 16) for item in process.memory_maps(grouped=False)
           if item.path.lower().endswith('gov-frun-dispatcher.exe')]
base = min(regions)
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
kernel.OpenProcess.restype = wintypes.HANDLE
kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel.ReadProcessMemory.restype = wintypes.BOOL
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
handle = kernel.OpenProcess(0x410, False, process.pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())

def read(rva, size):
    buf = (ctypes.c_ubyte * size)()
    done = ctypes.c_size_t()
    if not kernel.ReadProcessMemory(handle, base + rva, buf, size, ctypes.byref(done)) or done.value != size:
        raise ctypes.WinError(ctypes.get_last_error())
    return int.from_bytes(bytes(buf), 'little')

try:
    print({'pid': process.pid, 'base': hex(base), 'current_screen': read(0xa80970, 1),
           'requested_screen': read(0xa80971, 1), 'post_load_screen': read(0xa80972, 1),
           'generation_thread': hex(read(0xa80028, 8)), 'exit_screen_loop': read(0xa7f424, 1)})
finally:
    kernel.CloseHandle(handle)
