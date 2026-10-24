"""Complete original environment, AI-group and side owners; preserve game algorithms."""
import hashlib
import re


def prepare_ai_owners(output):
    changes={}
    for name,marker in [
        ('MatrixGame/src/Logic/MatrixEnvironment.cpp','CInfo::CInfo()'),
        ('MatrixGame/src/Logic/MatrixAIGroup.cpp','CMatrixGroup::CMatrixGroup()'),
        ('MatrixGame/src/MatrixSide.cpp','CMatrixSideUnit::CMatrixSideUnit()'),
    ]:
        path=output/name;text=path.read_text(encoding='utf-8')
        if text.count(marker)!=1:raise ValueError('Original AI owner changed: '+name)
        if name.endswith('/MatrixEnvironment.cpp'):
            shifts={
                'CopyMemory(m_BadPlace, m_BadPlace + 1, (8 - 1) * sizeof(int));':'std::memmove(m_BadPlace, m_BadPlace + 1, (8 - 1) * sizeof(m_BadPlace[0]));',
                'CopyMemory(m_BadCoord, m_BadCoord + 1, (16 - 1) * sizeof(int));':'std::memmove(m_BadCoord, m_BadCoord + 1, (16 - 1) * sizeof(m_BadCoord[0]));',
            }
            for before,after in shifts.items():
                if text.count(before)!=1:raise ValueError('Original AI FIFO shift changed: '+before)
                text=text.replace(before,after)
        if name.endswith('/MatrixSide.cpp'):
            clock='std::uint32_t ctime = timeGetTime();'
            if text.count(clock)!=1:raise ValueError('Original side statistics clock changed')
            text=text.replace(clock,'std::uint32_t ctime = srhd_awa::matrixgame::GameClockMilliseconds();')
            text=re.sub(r'\bTRUE\b','true',text);text=re.sub(r'\bFALSE\b','false',text)
        text=text.replace('MoveMemory(','std::memmove(').replace('CopyMemory(','std::memcpy(')
        prelude='#include "Texture.hpp"\n#include "game_clock.hpp"\n#include <cstring>\n'
        path.write_bytes((prelude+text).encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    # Future full map preparation and the side consumer must share the same
    # domain. Its selected cleanup TU does not use these timestamps.
    name='MatrixGame/src/MatrixMapPrepare.cpp';path=output/name
    text=path.read_text(encoding='utf-8')
    if text.count('timeGetTime()')!=2:raise ValueError('Original map start clocks changed')
    text='#include "game_clock.hpp"\n'+text.replace('timeGetTime()','srhd_awa::matrixgame::GameClockMilliseconds()')
    path.write_bytes(text.encode());changes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
    return changes
