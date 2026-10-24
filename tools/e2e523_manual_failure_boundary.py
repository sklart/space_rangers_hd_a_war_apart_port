"""Read-only constraints from historical error text and its exact producer."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
GIT=r'C:\Program Files\Git\cmd\git.exe'


def main():
    source=subprocess.check_output([GIT,'show','03651ce:port/switch/platform/e2e4_quicksave_transaction.cpp'],cwd=ROOT).decode()
    # These are the discriminating historical branches, not candidate wording.
    assert 'error = std::string(operation) + ": byte copy failed: " + ec.message() +' in source
    assert 'FileKind(to) != Kind::regular' in source
    assert 'if (!CopyBytesExclusive(from, to, ec) || ec ||' in source
    assert 'if (!ec) ec = {errno ? errno : EIO, std::generic_category()};' in source
    assert source.index('CopyVerified(temp, destination, next, fault, "manual-publish"') > source.index('if (!HashFile(temp, next) || !SyncFile(temp))')
    path=ROOT/'logs/2026.10.23/logs/perf515-stage.log'
    raw=path.read_bytes();lines=raw.decode(errors='replace').splitlines()
    expected='manual-publish: byte copy failed: No such file or directory, errno=No such file or directory'
    errors=[]
    for n,line in enumerate(lines,1):
        if expected not in line:continue
        errors.append({'line':n,'mono_us':int(re.search(r'mono_us=(\d+)',line)[1]),'text':line})
    assert len(errors)==2
    facts={
        'source_ref':'03651ce',
        'source_sha256':hashlib.sha256(source.encode()).hexdigest(),
        'log_sha256':hashlib.sha256(raw).hexdigest(),
        'errors':errors,
        'established':[
            'Both messages carry error_code ENOENT from CopyBytesExclusive, not merely trailing strerror(errno).',
            'A successful CopyBytesExclusive followed only by a failed destination FileKind would have ec=0 and message Success, not this ENOENT text.',
            'Source temp validation, hash/sync and manual journal preparation passed before entering manual-publish.',
            'Separate SyncFile after copy, copied size/CRC verification and manual committed marker were not reached.',
            'The source path is therefore not demonstrably persistently nonexistent; disappearance or VFS failure at later fopen remains possible.'
        ],
        'not_established':[
            'Which libc operation first failed: source fopen, target open, fdopen, read/write/flush/close.',
            'Whether ENOENT was the failing read error: old code did not reset errno and could report stale errno after ferror.',
            'Target native path bytes and equality of supplied resource filesystem with hardware filesystem.',
            'An exact historical syscall cause cannot be reconstructed from this common error text or RSG64 description alone.'
        ],
        'test_boundary':'Static historical source and existing hardware records; no new runtime PASS, no save fix claimed.'}
    out=ROOT/'reports/e2e5/hardware-20261023-manual-boundary.json'
    out.write_text(json.dumps(facts,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Two historical failures classified; exact syscall remains unknown.')


if __name__=='__main__':main()
