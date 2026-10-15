#include "win32_compat_gdi.hpp"

#include "win32_compat_window.hpp"
#include "win32_compat_messages.hpp"
#include "win32_handles.hpp"
#include "types/Windows_group.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#endif

namespace srhd_awa::platform::win32_compat {
namespace {
enum class Kind { Bitmap, Cursor, Icon, Brush };
struct Object {
  Kind kind;
  std::int32_t width = 0, height = 0;
  std::vector<std::uint8_t> pixels;
#if defined(__SWITCH__)
  SDL_Cursor* native_cursor = nullptr;
  ~Object() { if (native_cursor) SDL_FreeCursor(native_cursor); }
#endif
};
std::mutex g_mutex;
std::unordered_map<std::int32_t, std::uint32_t> g_stock;
std::uint32_t g_current_cursor = 0;
std::int32_t g_show_cursor = 0;
Types::TRect g_clip{};
bool g_has_clip = false;

std::shared_ptr<Object> Lookup(std::uint32_t token) {
  return std::static_pointer_cast<Object>(Handles().Lookup(token, HandleType::GdiObject));
}

std::uint32_t StockObject(std::int32_t index) {
  std::lock_guard lock(g_mutex);
  if (auto found = g_stock.find(index); found != g_stock.end()) return found->second;
  const auto token = Handles().Allocate(HandleType::GdiObject,
                                       std::make_shared<Object>(Kind::Brush));
  if (token) g_stock.emplace(index, token);
  return token;
}

std::int32_t DeleteObject(std::uint32_t token) {
  {
    std::lock_guard lock(g_mutex);
    for (const auto& [_, stock] : g_stock)
      if (stock == token) { SetLastError(kErrorInvalidHandle); return 0; }
  }
  return Handles().Close(token, HandleType::GdiObject) ? 1 : 0;
}

std::uint32_t CreateDib(std::uint32_t, const WindowsSdk::TBitmapInfo* info,
                        std::uint32_t, void** bits, std::uint32_t section,
                        std::uint32_t offset) {
  if (!info || !bits || section || offset) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  const auto& header = info->bmiHeader;
  const auto width = header.biWidth;
  const auto height = header.biHeight < 0 ? -header.biHeight : header.biHeight;
  if (width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
      header.biBitCount != 32 || header.biCompression != 0) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  auto object = std::make_shared<Object>(Kind::Bitmap);
  object->width = width; object->height = height;
  object->pixels.resize(static_cast<std::size_t>(width) * height * 4);
  *bits = object->pixels.data();
  return Handles().Allocate(HandleType::GdiObject, std::move(object));
}

std::uint32_t CreateIcon(const WindowsSdk::TIconInfo* info) {
  if (!info) { SetLastError(kErrorInvalidParameter); return 0; }
  auto object = std::make_shared<Object>(info->fIcon ? Kind::Icon : Kind::Cursor);
  if (const auto bitmap = Lookup(info->hbmColor)) {
    object->width = bitmap->width;
    object->height = bitmap->height;
    object->pixels = bitmap->pixels;
#if defined(__SWITCH__)
    if (!object->pixels.empty()) {
      auto* surface = SDL_CreateRGBSurfaceFrom(object->pixels.data(),
          object->width, object->height, 32, object->width * 4,
          0x00ff0000u, 0x0000ff00u, 0x000000ffu, 0xff000000u);
      if (surface) {
        object->native_cursor = SDL_CreateColorCursor(surface,
            static_cast<int>(info->xHotspot), static_cast<int>(info->yHotspot));
        SDL_FreeSurface(surface);
      }
    }
#endif
  }
  return Handles().Allocate(HandleType::GdiObject, std::move(object));
}

std::uint32_t LoadCursor(std::uint32_t, std::uint8_t*) {
  auto object = std::make_shared<Object>(Kind::Cursor);
#if defined(__SWITCH__)
  object->native_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
#endif
  return Handles().Allocate(HandleType::GdiObject, std::move(object));
}
std::uint32_t LoadIcon(std::uint32_t, std::uint8_t*) {
  return Handles().Allocate(HandleType::GdiObject,
                            std::make_shared<Object>(Kind::Icon));
}
std::int32_t DestroyIcon(std::uint32_t token) { return DeleteObject(token); }

std::uint32_t SetCursor(std::uint32_t token) {
  if (token && !Lookup(token)) return 0;
  std::lock_guard lock(g_mutex);
  const auto previous = g_current_cursor;
  g_current_cursor = token;
#if defined(__SWITCH__)
  if (token) {
    const auto object = Lookup(token);
    if (object && object->native_cursor) SDL_SetCursor(object->native_cursor);
  }
#endif
  return previous;
}

std::int32_t ShowCursor(std::int32_t show) {
  std::lock_guard lock(g_mutex);
  g_show_cursor += show ? 1 : -1;
#if defined(__SWITCH__)
  SDL_ShowCursor(g_show_cursor >= 0 ? SDL_ENABLE : SDL_DISABLE);
#endif
  return g_show_cursor;
}

std::int32_t SetCursorPos(std::int32_t x, std::int32_t y) {
  std::lock_guard lock(g_mutex);
  if (g_has_clip) {
    x = std::clamp(x, g_clip.Left, g_clip.Right - 1);
    y = std::clamp(y, g_clip.Top, g_clip.Bottom - 1);
  }
  WarpInputCursor(x, y);
  SetLastError(kErrorSuccess);
  return 1;
}
std::int32_t GetCursorPos(Types::TPoint* output) {
  if (!output) { SetLastError(kErrorInvalidParameter); return 0; }
  InputCursor(&output->X, &output->Y);
  return 1;
}
std::int32_t ClipCursor(const Types::TRect* rect) {
  std::lock_guard lock(g_mutex);
  if (rect) { g_clip = *rect; g_has_clip = true; }
  else g_has_clip = false;
  return 1;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveGdiImport(std::string_view dll, std::string_view symbol) {
  if (dll == "gdi32.dll") {
    if (symbol == "GetStockObject") return Address(&StockObject);
    if (symbol == "DeleteObject") return Address(&DeleteObject);
    if (symbol == "CreateDIBSection") return Address(&CreateDib);
  }
  if (dll == "user32.dll") {
    if (symbol == "LoadCursorA") return Address(&LoadCursor);
    if (symbol == "LoadIconA") return Address(&LoadIcon);
    if (symbol == "CreateIconIndirect") return Address(&CreateIcon);
    if (symbol == "DestroyIcon") return Address(&DestroyIcon);
    if (symbol == "SetCursor") return Address(&SetCursor);
    if (symbol == "ShowCursor") return Address(&ShowCursor);
    if (symbol == "SetCursorPos") return Address(&SetCursorPos);
    if (symbol == "GetCursorPos") return Address(&GetCursorPos);
    if (symbol == "ClipCursor") return Address(&ClipCursor);
  }
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
