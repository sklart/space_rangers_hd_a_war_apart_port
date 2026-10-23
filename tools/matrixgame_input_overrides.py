"""Remove map/input SDK-header dependencies without replacing original key state."""
import hashlib


def prepare_input(output):
    changes = {}
    name = 'MatrixGame/src/input.hpp'
    path = output / name
    text = path.read_text(encoding='utf-8')
    include = '#include <windows.h>'
    if text.count(include) != 1:
        raise ValueError('Original input SDK include changed')
    text = text.replace(include, '''// Original virtual-key protocol values, not SDL scancodes or a keyboard API.
namespace Input::VirtualKey {
inline constexpr uint8_t Space=0x20, Slash=0xbf, Tilde=0xc0, Period=0xbe,
                         LeftBracket=0xdb, RightBracket=0xdd;
}
#if defined(VK_OEM_2) && defined(VK_SPACE)
static_assert(Input::VirtualKey::Space==VK_SPACE && Input::VirtualKey::Slash==VK_OEM_2 &&
              Input::VirtualKey::Tilde==VK_OEM_3 && Input::VirtualKey::Period==VK_OEM_PERIOD &&
              Input::VirtualKey::LeftBracket==VK_OEM_4 && Input::VirtualKey::RightBracket==VK_OEM_6);
#endif''')
    for old, new in {
        '#define VK_LSLASH   VK_OEM_2':'#define VK_LSLASH   Input::VirtualKey::Slash',
        '#define VK_TILDA    VK_OEM_3':'#define VK_TILDA    Input::VirtualKey::Tilde',
        '#define VK_PERIOD   VK_OEM_PERIOD':'#define VK_PERIOD   Input::VirtualKey::Period',
        '#define VK_LBRACKET VK_OEM_4':'#define VK_LBRACKET Input::VirtualKey::LeftBracket',
        '#define VK_RBRACKET VK_OEM_6':'#define VK_RBRACKET Input::VirtualKey::RightBracket',
        'if (vk == VK_SPACE)':'if (vk == Input::VirtualKey::Space)',
    }.items():
        if text.count(old) != 1:
            raise ValueError('Original key-code boundary changed: ' + old)
        text = text.replace(old, new)
    path.write_bytes(text.encode())
    changes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    # SDK providers own their Windows includes. A shared game TU must not mix
    # SDK HWND/HINSTANCE with its already established opaque DLL identities.
    name = 'MatrixGame/src/MatrixMap.cpp'
    path = output / name
    text = path.read_text(encoding='utf-8')
    if text.count(include) != 1:
        raise ValueError('Original map SDK include changed')
    text = text.replace(include, '#include "platform_3g.hpp"')
    path.write_bytes(text.encode())
    changes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return changes
