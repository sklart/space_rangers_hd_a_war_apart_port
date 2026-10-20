"""Portable release compilation of the complete pinned road-network implementation."""
import hashlib
from pathlib import Path
import re

INPUTS = {
    'MatrixRoadNetwork.cpp': '4df1ab25b89462d040908ec96b1a49a456ff7d40373b767dfc8fd5ebd94c5b78',
    'MatrixRoadNetwork.hpp': '5c51ee811bc9a84ff40732a6df5dc12818364c1a1debe6d61cbc78efb1e653bf',
}

def patch_navigation(output: Path):
    results = {}
    for name, digest in INPUTS.items():
        path = output / 'MatrixGame/src/Logic' / name
        data = path.read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('Pinned navigation source changed: ' + name)
        text = data.decode()
        text = re.sub(r'\bDWORD\b', 'std::uint32_t', text)
        text = re.sub(r'\bBYTE\b', 'std::uint8_t', text)
        if name.endswith('.hpp'):
            if text.count('#include "Tracer.hpp"') != 1: raise ValueError('Navigation trace include changed')
            text = text.replace('#include "Tracer.hpp"', '#include <cstdint>')
        else:
            # DTRACE is empty in the original release build. Keep algorithms and
            # optional debug-visualization blocks intact; no renderer is faked.
            text = re.sub(r'\bDTRACE\(\);', '', text)
            text = text.replace('#include <algorithm>', '#include <algorithm>\n#include <cstring>')
            text = text.replace('CopyMemory(', 'std::memcpy(').replace('MoveMemory(', 'std::memmove(')
            for old, new in (
                ('ZeroMemory(m_Road, 8 * sizeof(CMatrixRoad *));', 'std::memset(m_Road, 0, 8 * sizeof(CMatrixRoad *));'),
                ('ZeroMemory(m_Place + no, sizeof(SMatrixPlace));', 'std::memset(m_Place + no, 0, sizeof(SMatrixPlace));')):
                if text.count(old) != 1: raise ValueError('Navigation zero boundary changed')
                text = text.replace(old, new)
        encoded = text.encode()
        path.write_bytes(encoded)
        results[path.relative_to(output).as_posix()] = hashlib.sha256(encoded).hexdigest()
    return results
