"""Exception safety at the package API boundary; pinned sources stay untouched."""
import re


def apply_file(text: str) -> str:
    lock = 'EC_HsFile::PackageFileLock'
    pattern = re.compile(r'pas::critical_enter\(' + re.escape(lock) +
                         r'\);(.*?)pas::critical_leave\(' + re.escape(lock) + r'\);', re.S)
    count = 0

    def guard(match):
        nonlocal count
        count += 1
        name = f'e2e_file_lock_{count}'
        return (f'srhd_awa::platform::FileLockGuard {name}({lock});' +
                match[1] + f'{name}.Unlock();')

    text = pattern.sub(guard, text)
    if count != 12 or 'pas::critical_enter(' in text:
        raise RuntimeError(f'EC_File lock boundaries changed: {count}')
    # GetSize acquires a nested handle reference even when querying fails.
    marker = '        AcquireReadWriteHandle();'
    assert text.count(marker) == 1
    text = text.replace(marker, marker + '''
        pas::ScopeExit release_handle = [&]() noexcept { ReleaseHandle(); };''')
    marker = '        ReleaseHandle();\n        return Size;'
    assert text.count(marker) == 1
    text = text.replace(marker, '        return Size;')
    return '#include "e2e58_file_lock.hpp"\n' + text


def apply_data(text: str) -> str:
    # Include AcquireReadWriteHandle itself in the protected lifetime.
    start = text.index('    void TDataEC::ReadEntryBuffer(')
    end = text.index('\n    }', start) + len('\n    }')
    body = text[start:end]
    body = body.replace('        pas::critical_enter(FileLock);',
                        '        srhd_awa::platform::FileLockGuard e2e_data_lock(FileLock);')
    body = body.replace('                pas::critical_leave(FileLock);', '')
    body = body.replace('            pas::critical_leave(FileLock);', '')
    return '#include "e2e58_file_lock.hpp"\n' + text[:start] + body + text[end:]
