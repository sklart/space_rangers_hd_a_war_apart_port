"""Wait for the queued save when the calling operation requires a commit."""

def apply(text):
    old = 'if (FileName != AutoName && FileName != TurnName && FileName != QuickName) {'
    new = '''if (srhd_awa::platform::e2e5_save_commit_wait::requested ||
                    (FileName != AutoName && FileName != TurnName && FileName != QuickName)) {'''
    entry = '    std::uint8_t SaveGameToFile(pas::WideString FileName, pas::WideString Description) {'
    if text.count(old) != 1 or text.count(entry) != 1:
        raise RuntimeError('save commit wait boundary changed')
    text = text.replace(old, new, 1)
    text = text.replace(entry, entry + '''
        srhd_awa::platform::e2e5_save_commit_wait::SaveCall save_call;
        if (!save_call.entered()) return false;''', 1)
    return '#include "e2e5_save_commit_wait.hpp"\n' + text
