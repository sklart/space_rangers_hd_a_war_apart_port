#pragma once
#include "win32_compat.hpp"
#include "input_platform.hpp"
#include <cstdint>
#include <array>
#include <string_view>
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveMessageImport(std::string_view dll, std::string_view symbol);
void InjectHostInput(input_platform::RawInput input);
void WarpInputCursor(std::int32_t x, std::int32_t y);
void PostPaint(std::uint32_t window);
std::uint64_t CurrentPressId();
bool CurrentMessageIsTouch();
struct ReleasedInput {
  std::array<bool, 256> keys{};
};
using ReleaseInputOwner = void (*)(void*, const ReleasedInput&) noexcept;
// One scope per real message loop. Clears queued input and cancels the previous
// owner's holds before the new loop can process events; never sends mouse-up
// to a new window, where it could activate a button without a matching down.
class InputContextScope {
public:
  InputContextScope(controller_actions::Context context, void* owner,
                    ReleaseInputOwner release);
  ~InputContextScope();
  void CancelHeldInput(); // Call before OnClose destroys gameplay resources.
  InputContextScope(const InputContextScope&) = delete;
  InputContextScope& operator=(const InputContextScope&) = delete;
private:
  InputContextScope* previous_{};
  controller_actions::Context context_{};
  void* owner_{};
  ReleaseInputOwner release_{};
};
#if defined(E2E5_HOST_SDL_WINDOW)
bool PostHostKey(std::uint32_t window, std::uint32_t virtual_key);
bool PostHostText(std::uint32_t window, std::u16string_view text);
bool PostHostReturnKey(std::uint32_t window);
bool PostHostLeftClick(std::uint32_t window, std::int32_t x, std::int32_t y);
#endif
}
