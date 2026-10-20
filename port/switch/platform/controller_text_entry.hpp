#pragma once
#include "e2e5_text_input.hpp"
#include "types/GI_Edit.hpp"

namespace srhd_awa::platform::controller_text_entry {
inline thread_local std::uint64_t last_pointer_press{};
inline bool Open(GI_Edit::TEditGI* edit) {
  if (!edit || !edit->Active || edit->MaxLength < 0) return false;
  const auto press = win32_compat::CurrentPressId();
  if (press && press == last_pointer_press) return false;
  last_pointer_press = press;
  auto candidate = edit->Text;
  const bool player = edit->ControlName == u"PlayerName";
  if (!e2e5_text_input::EditText(candidate, player ? 1 : 0,
          static_cast<std::size_t>(edit->MaxLength), player ? "Имя персонажа" : "Введите текст")) return false;
  for (int i = 1; i <= candidate.length(); ++i) {
    const auto ch = candidate.read(i);
    if (!edit->HasGlyph(ch) || (pas::assigned(edit->AcceptCharCallback) && !edit->AcceptCharCallback(edit, ch)))
      return false;
  }
  if (candidate == edit->Text) return true;
  edit->SetText(std::move(candidate));
  edit->SetCaretPosition(edit->Text.length());
  edit->DispatchNamedEvent(4, 0, 0);
  if (pas::assigned(edit->ChangedCallback)) edit->ChangedCallback(edit);
  return true;
}
} // namespace srhd_awa::platform::controller_text_entry
