#include "controller_text_input.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>

using namespace srhd_awa::platform::controller_text_input;
struct Keyboard {
  std::u16string initial, output;
  std::size_t maximum{};
  bool cancel{}, throws{};
};
bool BackendTest(const Request& request, std::u16string& output, void* user) {
  auto& keyboard = *static_cast<Keyboard*>(user);
  keyboard.initial = request.initial;
  keyboard.maximum = request.max_length;
  output = keyboard.output;
  if (keyboard.throws) throw std::runtime_error("keyboard unavailable");
  return !keyboard.cancel;
}
int main() {
  std::u16string value = u"  Старое Ё  ";
  Keyboard keyboard;
  keyboard.output = u"Русское Ё имя 123";
  assert(Prompt(value, 1, 55, "save", BackendTest, &keyboard));
  assert(value == keyboard.output && keyboard.initial == u"  Старое Ё  " && keyboard.maximum == 55);
  const auto previous = value;
  keyboard.cancel = true; keyboard.output = u"replacement";
  assert(!Prompt(value, 1, 55, "save", BackendTest, &keyboard) && value == previous);
  keyboard.cancel = false;
  for (const auto& invalid : {std::u16string(56, u'x'), std::u16string(u""),
      std::u16string(u"a\nb"), std::u16string{u'a', char16_t(0), u'b'},
      std::u16string(1, char16_t(0xd800)), std::u16string(1, char16_t(0xdc00))}) {
    keyboard.output = invalid;
    assert(!Prompt(value, 1, 55, "save", BackendTest, &keyboard) && value == previous);
  }
  keyboard.output = u"AB \U0001f680 Ё";
  assert(Prompt(value, 1, 55, "save", BackendTest, &keyboard) && value == keyboard.output);
  const auto before_exception = value;
  keyboard.throws = true;
  try { Prompt(value, 1, 55, "save", BackendTest, &keyboard); assert(false); }
  catch (const std::runtime_error&) {}
  assert(value == before_exception);
  keyboard.throws = false; keyboard.output = u"";
  assert(Prompt(value, 0, 256, "generic", BackendTest, &keyboard) && value.empty());
  keyboard.output = std::u16string(4096, u'Ё');
  assert(Prompt(value, 0, 4096, "long", BackendTest, &keyboard) && value.size() == 4096);
  assert(!Prompt(value, 0, 4097, "oversized", BackendTest, &keyboard));
  std::puts("PASS text input transaction: Unicode/Ё/spaces/long/Cancel/malformed UTF16/exception; no native applet execution");
}
