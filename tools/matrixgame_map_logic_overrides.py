"""Compile the complete original map logic through existing native contracts."""
import hashlib
import re


def prepare_map_logic(output):
    name = 'MatrixGame/src/MatrixLogic.cpp'
    path = output / name
    text = path.read_text(encoding='utf-8')
    replacements = {
        'UnionRect((LPRECT)&re, (LPRECT)&re, (LPRECT)&(m_RN.m_Zone[zonepath[i]].m_Rect));':
            're = srhd_awa::matrixgame::map_logic::UnionBounds(re, m_RN.m_Zone[zonepath[i]].m_Rect);',
        'D3DMATERIAL9 mtrl;':
            'srhd_awa::matrixgame::platform3g::VideoMaterial mtrl;',
        '::srhd_awa::matrixgame::ZeroTrivialBytes(&mtrl, sizeof(D3DMATERIAL9));': 'mtrl = {};',
        'g_D3DD->SetMaterial(&mtrl);':
            'srhd_awa::matrixgame::platform3g::ReadyDevice().SetVideoMaterial(mtrl);',
    }
    for old, new in replacements.items():
        if text.count(old) != 1:
            raise ValueError('Original MatrixLogic boundary changed: ' + old)
        text = text.replace(old, new)
    text, copies = re.subn(r'(?m)^(\s*)MoveMemory\(', r'\1std::memmove(', text)
    if copies != 3 or text.count('_alloca(sz)') != 2:
        raise ValueError('Original map path copy/stack allocation boundaries changed')
    text = text.replace('_alloca(sz)', 'MATRIXGAME_LOGIC_STACK_ALLOC(sz)')
    prelude = '#include "map_logic_backend.hpp"\n#include "platform_3g.hpp"\n#include <cstring>\n'
    path.write_bytes((prelude + text).encode())
    return {name: hashlib.sha256(path.read_bytes()).hexdigest()}
