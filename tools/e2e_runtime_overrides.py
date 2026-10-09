"""Create a disposable ARM64 runtime header with portable named heaps."""

from pathlib import Path
import filecmp
import shutil


def generated_windows_header(source: Path, destination: Path) -> Path:
    text = source.read_text(encoding="utf-8")
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
      std::free(it->first);
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
  void *p = std::malloc(std::max<std::size_t>(bytes, 1));
  if (p) {
    if (flags & 8) std::memset(p, 0, bytes);
    heap_allocations.emplace(p, HeapAllocation{heap, bytes});
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
  void *p = std::realloc(data, std::max<std::size_t>(bytes, 1));
  if (p) {
    heap_allocations.erase(found);
    heap_allocations.emplace(p, HeapAllocation{heap, bytes});
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
  std::free(data);
  return true;
}
#endif
'''
    destination.parent.mkdir(parents=True, exist_ok=True)
    rendered = prefix + replacement + suffix
    if not destination.exists() or destination.read_text(encoding="utf-8") != rendered:
        destination.write_text(rendered, encoding="utf-8")
    return destination


def prepare_runtime_overlay(runtime_root: Path, overlay: Path) -> Path:
    generated_windows_header(runtime_root / "windows.hpp", overlay / "windows.hpp")
    for name in ("float_text.hpp", "graphics.hpp", "locale.hpp"):
        source = runtime_root / name
        destination = overlay / name
        if not destination.exists() or not filecmp.cmp(source, destination, shallow=False):
            shutil.copyfile(source, destination)
    return overlay
