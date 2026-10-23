"""Preserve real MatrixCursor construction/Clear via borrowed Window operations."""
import hashlib
import re
from matrixgame_vector_object_overrides import function

def prepare_cursor(output):
    name='MatrixGame/src/MatrixCursor.hpp';path=output/name
    h=path.read_text(encoding='utf-8')
    cpath=output/'MatrixGame/src/MatrixCursor.cpp';c=cpath.read_text(encoding='utf-8')
    ctor=function(h,'    CMatrixCursor(void)')
    # Zero scalars explicitly, preserving the original values without corrupting
    # a live wstring. The complete original member layout is retained, plus the
    # borrowed typed Window needed after the platform state is detached.
    h=h.replace('#include "MatrixMap.hpp"','''#pragma once
#include "CMain.hpp"
#include "platform_3g.hpp"
#include <string>
class CTextureManaged;''')
    h=re.sub(r'\bHICON\b','srhd_awa::matrixgame::platform3g::IconHandle',h)
    h=re.sub(r'\bHCURSOR\b','srhd_awa::matrixgame::platform3g::CursorHandle',h)
    h=h.replace(ctor,'    CMatrixCursor(void);')
    h=h.replace('    int m_Time;','    int m_Time;\n    srhd_awa::matrixgame::platform3g::Window* m_CursorWindow;')
    ctor='''CMatrixCursor::CMatrixCursor(void)
    :m_CursorFlags(0),m_Pos(0,0),m_HotSpot(0,0),m_CursorTexture(nullptr),m_CursorSize(0),
     m_CursorIcons(nullptr),m_OldCursor{},m_FramesCnt(0),m_FrameInc(0),m_Frame(0),m_NextCursorTime(0),
     m_CursorTimePeriod(0),m_CursorInTexLine(0),m_TexSizeXInversed(0),m_TexSizeYInversed(0),
     m_u0(0),m_v0(0),m_u1(0),m_v1(0),m_Time(0),m_CursorWindow(&srhd_awa::matrixgame::platform3g::ReadyWindow()) {
    m_OldCursor=m_CursorWindow->ExchangeCursor({});
}
'''
    clear=function(c,'void CMatrixCursor::Clear(void)')
    native_clear=clear.replace('SetCursor(m_OldCursor);','m_CursorWindow->ExchangeCursor(m_OldCursor);')
    native_clear=native_clear.replace('DestroyIcon(m_CursorIcons[i]);','m_CursorWindow->DestroyCursorIcon(m_CursorIcons[i]);')
    # Keep the genuine virtual Unload contract while the concrete legacy texture
    # backend is still unported. No dummy Unload definition is introduced.
    native_clear=native_clear.replace('m_CursorTexture->Unload();','static_cast<CCacheData*>(m_CursorTexture)->Unload();')
    c=c.replace(clear,'// Clear moved to MatrixCursorLifecycle.cpp; original body retained there.')
    # Selection/Draw remain the original unsupported implementation, not stubs.
    lifecycle=('''// Original MatrixCursor lifecycle, GPL-2.0-or-later; not full cursor TU.
#include "MatrixCursor.hpp"
#include "Texture.hpp"
#include "MatrixGame.h"
'''+ctor+'\n'+native_clear+'\n').encode()
    path.write_bytes(h.encode());cpath.write_bytes(c.encode())
    generated='MatrixGame/src/MatrixCursorLifecycle.cpp';(output/generated).write_bytes(lifecycle)
    return {name:hashlib.sha256(h.encode()).hexdigest(),'MatrixGame/src/MatrixCursor.cpp':hashlib.sha256(c.encode()).hexdigest(),generated:hashlib.sha256(lifecycle).hexdigest()}
