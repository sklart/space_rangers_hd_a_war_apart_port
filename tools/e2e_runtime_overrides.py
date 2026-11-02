"""Create disposable portable runtime headers and named heaps."""

from pathlib import Path
import filecmp
import shutil


def generated_windows_header(source: Path, destination: Path, portable_windows_host: bool = False) -> Path:
    text = source.read_text(encoding="utf-8")
    original_loader = '''inline auto resolve_import(const char *library, const char *) -> void (*)() {
  throw std::runtime_error(std::string("DLL import requires a Win32 host: ") +
                           library);
}'''
    switch_loader = '''inline auto resolve_import(const char *library, const char *name) -> void (*)() {
#if defined(__SWITCH__) || defined(E2E_HOST_RESOLVER_TEST)
  return srhd_awa::platform::win32_compat::ResolveImport(library, name);
#else
  throw std::runtime_error(std::string("DLL import requires a Win32 host: ") +
                           library);
#endif
}'''
    if text.count(original_loader) != 1:
        raise RuntimeError("upstream Win32 loader boundary changed")
    text = text.replace(original_loader, switch_loader, 1)
    text = '#if defined(__SWITCH__) || defined(E2E_HOST_RESOLVER_TEST)\n#include "win32_compat.hpp"\n#endif\n' + text
    begin = "inline std::mutex heap_mutex;"
    end = "#endif\n"
    if text.count(begin) != 1 or text.rfind(end) < text.index(begin):
        raise RuntimeError("upstream portable heap boundary changed")
    prefix = text[:text.index(begin)]
    suffix = text[text.rfind(end) + len(end):]
    replacement = '''inline std::mutex heap_mutex;
struct HeapAllocation { Cardinal owner; std::size_t size; };
inline std::unordered_map<void *, HeapAllocation> heap_allocations;
inline std::unordered_map<Cardinal, bool> private_heaps;
inline Cardinal next_private_heap = 2;
inline Cardinal process_heap() { return 1; }
inline Cardinal heap_create(Cardinal flags, Cardinal, Cardinal) {
  if (flags & ~Cardinal(1)) return 0;
  std::lock_guard lock(heap_mutex);
  if (next_private_heap == 0) return 0;
  const Cardinal handle = next_private_heap++;
  private_heaps.emplace(handle, true);
  return handle;
}
inline bool heap_destroy(Cardinal heap) {
  std::lock_guard lock(heap_mutex);
  if (!private_heaps.erase(heap)) return false;
  for (auto it = heap_allocations.begin(); it != heap_allocations.end();) {
    if (it->second.owner == heap) {
      srhd_awa::platform::heap_reserve::Free(it->first);
      it = heap_allocations.erase(it);
    } else {
      ++it;
    }
  }
  return true;
}
inline void check_heap(Cardinal heap, Cardinal flags) {
  if ((heap != process_heap() && !private_heaps.contains(heap)) ||
      (flags & ~Cardinal(8)))
    throw std::runtime_error("invalid native heap or flags");
}
inline void *heap_alloc(Cardinal heap, Cardinal flags, Cardinal bytes) {
  std::lock_guard lock(heap_mutex);
  check_heap(heap, flags);
  srhd_awa::platform::heap_reserve::Initialize();
  void *p = srhd_awa::platform::heap_reserve::Allocate(std::max<std::size_t>(bytes, 1));
  if (p) {
    if (flags & 8) std::memset(p, 0, bytes);
    try {
      heap_allocations.emplace(p, HeapAllocation{heap, bytes});
    } catch (...) {
      srhd_awa::platform::heap_reserve::Free(p);
      throw;
    }
  }
  return p;
}
inline void *heap_realloc(Cardinal heap, Cardinal flags, void *data,
                          Cardinal bytes) {
  std::lock_guard lock(heap_mutex);
  check_heap(heap, flags);
  auto found = heap_allocations.find(data);
  if (found == heap_allocations.end() || found->second.owner != heap)
    throw std::runtime_error("invalid native heap realloc");
  const auto old_size = found->second.size;
  void *p = srhd_awa::platform::heap_reserve::Reallocate(data,
      std::max<std::size_t>(bytes, 1), old_size);
  if (p) {
    // Reuse the tracking node. Reallocating this node on every buffer resize
    // creates allocator churn and can throw after realloc already freed data.
    auto node = heap_allocations.extract(found);
    node.key() = p;
    node.mapped().size = bytes;
    heap_allocations.insert(std::move(node));
    if ((flags & 8) && bytes > old_size)
      std::memset(static_cast<std::uint8_t *>(p) + old_size, 0,
                  bytes - old_size);
  }
  return p;
}
inline bool heap_free(Cardinal heap, Cardinal flags, void *data) {
  std::lock_guard lock(heap_mutex);
  check_heap(heap, flags);
  if (!data) return true;
  auto found = heap_allocations.find(data);
  if (found == heap_allocations.end() || found->second.owner != heap)
    throw std::runtime_error("invalid native heap free");
  heap_allocations.erase(found);
  srhd_awa::platform::heap_reserve::Free(data);
  return true;
}
#endif
'''
    destination.parent.mkdir(parents=True, exist_ok=True)
    helper = Path(__file__).resolve().parents[1] / "port/switch/platform/e2e510_heap_reserve.hpp"
    helper_copy = destination.parent / helper.name
    if not helper_copy.exists() or not filecmp.cmp(helper, helper_copy, shallow=False):
        shutil.copyfile(helper, helper_copy)
    rendered = '#include "e2e510_heap_reserve.hpp"\n' + prefix + replacement + suffix
    if portable_windows_host:
        # Explicit test mode must use the port's real handle adapters on a
        # Windows host too; native Windows without this mode is unchanged.
        rendered = rendered.replace('#ifdef _WIN32', '#if defined(_WIN32) && !defined(E2E_HOST_RESOLVER_TEST)')
        rendered = rendered.replace('#ifndef _WIN32', '#if !defined(_WIN32) || defined(E2E_HOST_RESOLVER_TEST)')
    if not destination.exists() or destination.read_text(encoding="utf-8") != rendered:
        destination.write_text(rendered, encoding="utf-8")
    return destination


def prepare_runtime_overlay(runtime_root: Path, overlay: Path, portable_windows_host: bool = False) -> Path:
    generated_windows_header(runtime_root / "windows.hpp", overlay / "windows.hpp", portable_windows_host)
    names = [path.name for path in runtime_root.glob("*.hpp") if path.name != "windows.hpp"] if portable_windows_host else ("float_text.hpp", "graphics.hpp", "locale.hpp")
    for name in names:
        source = runtime_root / name
        destination = overlay / name
        if not destination.exists() or not filecmp.cmp(source, destination, shallow=False):
            shutil.copyfile(source, destination)
    if portable_windows_host:
        # MSVC searches the active quoted-include stack before /I paths. A
        # complete private runtime keeps nested headers on the same overlay.
        shutil.copyfile(runtime_root / "runtime.cpp", overlay / "runtime.cpp")
        shutil.copyfile(runtime_root.parent / "src/runtime_support.hpp", overlay / "runtime_support.hpp")
        # The upstream Win32 storage describes 32-bit CRITICAL_SECTION (24
        # bytes). The Windows x64 ABI requires 40 bytes, aligned to a pointer.
        # Keep the real native API and only correct its private host storage.
        sync = (runtime_root / "sync.hpp").read_text(encoding="utf-8")
        old = "alignas(4) std::byte section[24]{};"
        if sync.count(old) != 1:
            raise RuntimeError("upstream native critical-section layout changed")
        sync = sync.replace(old, "alignas(sizeof(void*)) std::byte section[sizeof(void*) == 8 ? 40 : 24]{};", 1)
        (overlay / "sync.hpp").write_text(sync, encoding="utf-8")
    return overlay
