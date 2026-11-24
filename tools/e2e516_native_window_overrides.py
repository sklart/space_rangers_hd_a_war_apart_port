"""Allow the real E2E startup to borrow the already owned native window."""

def apply(text):
    start = text.index('    void ShowAndFocusMainWindow() {')
    end = text.index('    void LoadDatConfigAndModOverrides()', start)
    body = text[start:end]
    old = '''        if (window == nullptr) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=runtime/settings SDL-window-missing");'''
    new = '''        if (window == nullptr && srhd_awa::platform::renderer_platform::CurrentRgb565Presenter()) {
            srhd_awa::platform::e2e_stage::Log("runtime/settings native presenter already owns window");
            return;
        }
        if (window == nullptr) {
            srhd_awa::platform::e2e_stage::Log("FAIL stage=runtime/settings SDL-window-missing");'''
    if body.count(old) != 1:
        raise RuntimeError('E2E516 window focus boundary changed')
    body = body.replace(old, new, 1)
    return '#include "rgb565_presentation.hpp"\n' + text[:start] + body + text[end:]
