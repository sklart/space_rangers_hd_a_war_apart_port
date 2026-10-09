#include "win32_compat_window.hpp"

#include "e2e_stage.hpp"
#include "win32_compat_messages.hpp"
#include "win32_handles.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#endif

namespace srhd_awa::platform::win32_compat {
namespace {
struct Window {
  void* native = nullptr;
  std::int32_t x = 0, y = 0, width = 1280, height = 720;
  std::uint32_t style = 0;
  bool visible = true, focused = true;
  std::string title;
  WindowCallback proc = nullptr;
};
std::mutex g_mutex;
std::uint32_t g_main = 0;
std::uint32_t g_focus = 0;
std::int32_t g_cursor_x = 640, g_cursor_y = 360;
std::int32_t g_cursor_visibility = 0;
std::unordered_map<std::u16string, WindowCallback> g_classes;

std::shared_ptr<Window> Lookup(std::uint32_t token) {
  return std::static_pointer_cast<Window>(Handles().Lookup(token, HandleType::Window));
}

std::uint16_t RegisterClass(const WindowsSdk::TWndClassW* info) {
  if (!info || !info->lpszClassName) { SetLastError(kErrorInvalidParameter); return 0; }
  std::lock_guard lock(g_mutex);
  g_classes[info->lpszClassName] = reinterpret_cast<WindowCallback>(info->lpfnWndProc);
  SetLastError(kErrorSuccess);
  return 1;
}

std::uint32_t CreateWindow(std::uint32_t, char16_t* class_name,
    char16_t* title, std::uint32_t style, std::int32_t x, std::int32_t y,
    std::int32_t width, std::int32_t height, std::uint32_t, std::uint32_t,
    std::uint32_t, void*) {
  auto state = std::make_shared<Window>();
  state->x = x; state->y = y;
  state->width = std::max(1, width); state->height = std::max(1, height);
  state->style = style;
  if (title) for (const auto* p = title; *p; ++p)
    state->title.push_back(*p <= 127 ? static_cast<char>(*p) : '?');
  {
    std::lock_guard lock(g_mutex);
    if (class_name) {
      const auto found = g_classes.find(class_name);
      if (found != g_classes.end()) state->proc = found->second;
    }
  }
  const auto token = Handles().Allocate(HandleType::Window, std::move(state));
  if (!token) return 0;
  std::lock_guard lock(g_mutex);
  if (!g_main) g_main = token;
  g_focus = token;
  return token;
}

std::int32_t DestroyWindow(std::uint32_t token) {
  if (!Handles().Close(token, HandleType::Window)) return 0;
  std::lock_guard lock(g_mutex);
  if (g_main == token) g_main = 0;
  if (g_focus == token) g_focus = 0;
  return 1;
}

std::uint32_t ActiveWindow() {
  std::lock_guard lock(g_mutex);
  return g_focus ? g_focus : g_main;
}

std::int32_t ShowWindow(std::uint32_t token, std::int32_t command) {
  const auto state = Lookup(token);
  if (!state) return 0;
  const bool previous = state->visible;
  state->visible = command != 0;
#if defined(__SWITCH__)
  if (state->native) {
    if (state->visible) SDL_ShowWindow(static_cast<SDL_Window*>(state->native));
    else SDL_HideWindow(static_cast<SDL_Window*>(state->native));
  }
#endif
  SetLastError(kErrorSuccess);
  return previous ? 1 : 0;
}

std::int32_t SetWindowPos(std::uint32_t token, std::uint32_t,
    std::int32_t x, std::int32_t y, std::int32_t width,
    std::int32_t height, std::uint32_t flags) {
  const auto state = Lookup(token);
  if (!state) return 0;
  if (!(flags & 0x2u)) { state->x = x; state->y = y; }
  if (!(flags & 0x1u)) { state->width = std::max(1, width); state->height = std::max(1, height); }
  if (flags & 0x40u) state->visible = true;
#if defined(__SWITCH__)
  if (state->native) {
    auto* window = static_cast<SDL_Window*>(state->native);
    SDL_SetWindowPosition(window, state->x, state->y);
    SDL_SetWindowSize(window, state->width, state->height);
    if (state->visible) SDL_ShowWindow(window);
  }
#endif
  SetLastError(kErrorSuccess);
  return 1;
}

std::uint32_t SetFocus(std::uint32_t token) {
  if (!Lookup(token)) return 0;
  std::lock_guard lock(g_mutex);
  const auto previous = g_focus;
  g_focus = token;
  SetLastError(kErrorSuccess);
  return previous;
}

std::int32_t SetForeground(std::uint32_t token) {
  const auto state = Lookup(token);
  if (!state) return 0;
  {
    std::lock_guard lock(g_mutex);
    g_focus = token;
  }
#if defined(__SWITCH__)
  if (state->native) SDL_RaiseWindow(static_cast<SDL_Window*>(state->native));
#endif
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t UpdateWindow(std::uint32_t token) {
  if (!Lookup(token)) return 0;
  PostPaint(token);
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t RedrawWindow(std::uint32_t token, Types::TRect*,
                           std::uint32_t, std::uint32_t) {
  return UpdateWindow(token);
}

std::int32_t SetWindowText(std::uint32_t token, std::uint8_t* title) {
  const auto state = Lookup(token);
  if (!state || !title) { SetLastError(kErrorInvalidParameter); return 0; }
  state->title = reinterpret_cast<const char*>(title);
#if defined(__SWITCH__)
  if (state->native) SDL_SetWindowTitle(static_cast<SDL_Window*>(state->native),
                                         state->title.c_str());
#endif
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t AdjustWindowRect(Types::TRect* rect, std::uint32_t, std::int32_t) {
  if (!rect) { SetLastError(kErrorInvalidParameter); return 0; }
  SetLastError(kErrorSuccess); // SDL full-screen client and window rectangles coincide.
  return 1;
}

std::int32_t ClientToScreen(std::uint32_t token, Types::TPoint* point) {
  const auto state = Lookup(token);
  if (!state || !point) { SetLastError(kErrorInvalidParameter); return 0; }
  point->X += state->x; point->Y += state->y;
  SetLastError(kErrorSuccess);
  return 1;
}
std::int32_t ScreenToClient(std::uint32_t token, Types::TPoint* point) {
  const auto state = Lookup(token);
  if (!state || !point) { SetLastError(kErrorInvalidParameter); return 0; }
  point->X -= state->x; point->Y -= state->y;
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t IntersectRect(Types::TRect* dest, const Types::TRect* a,
                           const Types::TRect* b) {
  if (!dest || !a || !b) { SetLastError(kErrorInvalidParameter); return 0; }
  *dest = {std::max(a->Left, b->Left), std::max(a->Top, b->Top),
           std::min(a->Right, b->Right), std::min(a->Bottom, b->Bottom)};
  if (dest->Left >= dest->Right || dest->Top >= dest->Bottom) {
    *dest = {};
    return 0;
  }
  return 1;
}
std::int32_t UnionRect(Types::TRect* dest, const Types::TRect* a,
                       const Types::TRect* b) {
  if (!dest || !a || !b) { SetLastError(kErrorInvalidParameter); return 0; }
  *dest = {std::min(a->Left, b->Left), std::min(a->Top, b->Top),
           std::max(a->Right, b->Right), std::max(a->Bottom, b->Bottom)};
  return 1;
}

std::int32_t SetWindowLong(std::uint32_t token, std::int32_t index,
                            std::int32_t value) {
  const auto state = Lookup(token);
  if (!state || index != -16) { SetLastError(kErrorInvalidParameter); return 0; }
  const auto previous = state->style;
  state->style = static_cast<std::uint32_t>(value);
  return static_cast<std::int32_t>(previous);
}

std::uint32_t GetDC(std::uint32_t token) {
  if (token && !Lookup(token)) return 0;
  return Handles().Allocate(HandleType::GdiObject,
                            std::make_shared<std::uint32_t>(token));
}
std::int32_t ReleaseDC(std::uint32_t token, std::uint32_t dc) {
  if (token && !Lookup(token)) return 0;
  return Handles().Close(dc, HandleType::GdiObject) ? 1 : 0;
}

std::uint32_t DoubleClickTime() { return 500; }

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

std::uint32_t RegisterMainWindow(void* native_window, std::int32_t width,
                                 std::int32_t height) {
  auto state = std::make_shared<Window>();
  state->native = native_window;
  state->width = width; state->height = height;
  state->title = "Space Rangers HD: A War Apart";
  const auto token = Handles().Allocate(HandleType::Window, std::move(state));
  if (!token) return 0;
  std::lock_guard lock(g_mutex);
  g_main = g_focus = token;
  return token;
}
bool BindMainWindowProc(std::uint32_t token, WindowCallback proc) {
  const auto state = Lookup(token);
  if (!state || !proc || token != MainWindow()) {
    SetLastError(kErrorInvalidParameter);
    return false;
  }
  state->proc = proc;
  SetLastError(kErrorSuccess);
  return true;
}
void UnregisterMainWindow(std::uint32_t token) { DestroyWindow(token); }
std::uint32_t MainWindow() {
  std::lock_guard lock(g_mutex);
  return g_main;
}
void MainWindowSize(std::int32_t* width, std::int32_t* height) {
  const auto state = Lookup(MainWindow());
  if (width) *width = state ? state->width : 1280;
  if (height) *height = state ? state->height : 720;
}
void SetInputCursor(std::int32_t x, std::int32_t y) {
  std::lock_guard lock(g_mutex);
  g_cursor_x = x; g_cursor_y = y;
}
void InputCursor(std::int32_t* x, std::int32_t* y) {
  std::lock_guard lock(g_mutex);
  if (x) *x = g_cursor_x;
  if (y) *y = g_cursor_y;
}
std::int32_t DispatchWindowMessage(const Windows::TMsg& message) {
  const auto state = Lookup(message.hwnd);
  if (!state) return 0;
  const auto proc = state->proc;
  return proc ? proc(message.hwnd, message.message,
                     static_cast<std::int32_t>(message.wParam), message.lParam) : 0;
}

ImportAddress ResolveWindowImport(std::string_view dll, std::string_view symbol) {
  if (dll != "user32.dll") return nullptr;
  if (symbol == "GetActiveWindow") return Address(&ActiveWindow);
  if (symbol == "CreateWindowExW") return Address(&CreateWindow);
  if (symbol == "RegisterClassW") return Address(&RegisterClass);
  if (symbol == "DestroyWindow") return Address(&DestroyWindow);
  if (symbol == "ShowWindow") return Address(&ShowWindow);
  if (symbol == "SetWindowPos") return Address(&SetWindowPos);
  if (symbol == "SetFocus") return Address(&SetFocus);
  if (symbol == "GetForegroundWindow") return Address(&ActiveWindow);
  if (symbol == "SetForegroundWindow") return Address(&SetForeground);
  if (symbol == "UpdateWindow") return Address(&UpdateWindow);
  if (symbol == "RedrawWindow") return Address(&RedrawWindow);
  if (symbol == "SetWindowTextA") return Address(&SetWindowText);
  if (symbol == "AdjustWindowRect") return Address(&AdjustWindowRect);
  if (symbol == "ClientToScreen") return Address(&ClientToScreen);
  if (symbol == "ScreenToClient") return Address(&ScreenToClient);
  if (symbol == "IntersectRect") return Address(&IntersectRect);
  if (symbol == "UnionRect") return Address(&UnionRect);
  if (symbol == "SetWindowLongA") return Address(&SetWindowLong);
  if (symbol == "GetDC") return Address(&GetDC);
  if (symbol == "ReleaseDC") return Address(&ReleaseDC);
  if (symbol == "GetDoubleClickTime") return Address(&DoubleClickTime);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
