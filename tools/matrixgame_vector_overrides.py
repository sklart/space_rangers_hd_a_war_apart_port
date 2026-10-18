"""Extract actual VO payload layouts without device/cache dependencies."""
import re

INPUTS = {
    'VectorObject.hpp': '41af7cf50ab96af88b57ca5101cdf1b27d25ce6326517d532056142912ee0b72',
    'VectorObject.cpp': '11ffb4eb842b5cdf6041d0ddc3bf25a124b06bdd8ac36f090b4c2ac6d9586303',
}
SUBDIRS = {name: '../3G' for name in INPUTS}


def patch_vector(files):
    source = files['VectorObject.hpp'].decode()
    names = ['SVOVertex', 'SVOTriangle', 'SVOUnion', 'SVOKadr', 'SVOFrameIndex',
             'SVONormal', 'SVOKadrEdge', 'SVOGeometrySimple']
    parts = []
    for name in names:
        start = source.index('struct ' + name + ' {')
        opening = source.index('{', start)
        depth, end = 1, opening + 1
        while depth:
            if source[end] == '{': depth += 1
            if source[end] == '}': depth -= 1
            end += 1
        text = source[start:end + 1]
        # FVF is a renderer layout selector, not part of a serialized vertex.
        text = text.replace('    static const DWORD FVF = VO_FVF;\n', '')
        for old, new in [('DWORD', 'std::uint32_t'), ('BYTE', 'std::uint8_t')]:
            text = re.sub(r'\b' + old + r'\b', new, text)
        parts.append(text)
    header = source[:source.index('#pragma once')]
    header += '#pragma once\n#include "Math3D.hpp"\n#include <cstdint>\n'
    header += 'namespace srhd_awa::matrixgame::vo {\n' + '\n'.join(parts) + '\n}\n'
    files['VectorLayout.hpp'] = header.encode()
